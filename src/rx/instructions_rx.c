/*
 **********************************************************************************************
 * Renesas RX instruction set simulator  
 *
 * State: Nearly complete, one Code review done. Thread Pool OS starts up and schedules.
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without modification, are
 * permitted provided that the following conditions are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice, this list of
 *       conditions and the following disclaimer.
 *
 *    2. Redistributions in binary form must reproduce the above copyright notice, this list
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
 **********************************************************************************************
 */

#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <inttypes.h>
#include "instructions_rx.h"
#include "cpu_rx.h"
#include "byteorder.h"
#include "softfloat.h"

#define ISNEG(x) ((x)&(UINT32_C(1) << 31))
#define ISNOTNEG(x) (!((x)&(UINT32_C(1) << 31)))
#define ISNEG64(x) ((x)&(UINT64_C(1) << 63))
#define ISNOTNEG64(x) (!((x)&(UINT64_C(1) << 63)))

static inline uint16_t
add32_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
    if (((ISNEG(op1) && ISNEG(op2))
         || (ISNEG(op1) && ISNOTNEG(result))
         || (ISNEG(op2) && ISNOTNEG(result)))) {
        return PSW_C;
    } else {
        return 0;
    }
}

static inline uint16_t
add32_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
    if ((ISNEG(op1) && ISNEG(op2) && ISNOTNEG(result))
        || (ISNOTNEG(op1) && ISNOTNEG(op2) && ISNEG(result))) {
        return PSW_O;
    } else {
        return 0;
    }
}

/* Index Bit0 : op1, Bit 1: op2, Bit 2: result */
static const uint8_t add_flagtab[8] = {
    0,
    PSW_C,
    PSW_C,
    PSW_O | PSW_C,
    PSW_S | PSW_O,
    PSW_S,
    PSW_S,
    PSW_S | PSW_C
};

static inline void
add_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
    unsigned int index = (op1 >> 31) | ((op2 >> 31) << 1) | ((result >> 31) << 2);
    uint8_t flags;
    flags = add_flagtab[index];
    if (result == 0) {
        flags |= PSW_Z;
    }
    RX_REG_PSW = (RX_REG_PSW & ~(PSW_O | PSW_S | PSW_Z | PSW_C)) | flags;
}

static inline uint32_t
sub32_carry(uint32_t op1, uint32_t op2, uint32_t result)
{
    if (((ISNEG(op1) && ISNOTNEG(op2))
         || (ISNEG(op1) && ISNOTNEG(result))
         || (ISNOTNEG(op2) && ISNOTNEG(result)))) {
        return PSW_C;
    } else {
        return 0;
    }
}

static inline uint32_t
sub32_overflow(uint32_t op1, uint32_t op2, uint32_t result)
{
    if ((ISNEG(op1) && ISNOTNEG(op2) && ISNOTNEG(result))
        || (ISNOTNEG(op1) && ISNEG(op2) && ISNEG(result))) {
        return PSW_O;
    } else {
        return 0;
    }
}

static inline void
sub_flags(uint32_t op1, uint32_t op2, uint32_t result)
{
    RX_REG_PSW &= ~(PSW_O | PSW_S | PSW_Z | PSW_C);
    RX_REG_PSW |= sub32_carry(op1, op2, result);
    RX_REG_PSW |= sub32_overflow(op1, op2, result);
    if (result == 0) {
        RX_REG_PSW |= PSW_Z;
    } else if (ISNEG(result)) {
        RX_REG_PSW |= PSW_S;
    }
}

static inline void
and_flags(uint32_t result)
{
    RX_REG_PSW &= ~(PSW_S | PSW_Z);
    if (ISNEG(result)) {
        RX_REG_PSW |= PSW_S;
    } else if (result == 0) {
        RX_REG_PSW |= PSW_Z;
    }
}

static inline void
or_flags(uint32_t result)
{
    RX_REG_PSW &= ~(PSW_S | PSW_Z);
    if (ISNEG(result)) {
        RX_REG_PSW |= PSW_S;
    } else if (result == 0) {
        RX_REG_PSW |= PSW_Z;
    }
}

static inline void
not_flags(uint32_t result)
{
    RX_REG_PSW &= ~(PSW_S | PSW_Z);
    if (ISNEG(result)) {
        RX_REG_PSW |= PSW_S;
    } else if (result == 0) {
        RX_REG_PSW |= PSW_Z;
    }
}

static inline void
xor_flags(uint32_t result)
{
    RX_REG_PSW &= ~(PSW_S | PSW_Z);
    if (ISNEG(result)) {
        RX_REG_PSW |= PSW_S;
    } else if (result == 0) {
        RX_REG_PSW |= PSW_Z;
    }
}

static inline int32_t
RX_ReadSimm8(uint32_t addr)
{
    int8_t simm8 = RX_Read8(addr);
    return simm8;
}

static inline int32_t
RX_ReadSimm16(uint32_t addr)
{
    int16_t simm16 = RX_Read16(addr);
    return simm16;
}

static inline int32_t
RX_ReadSimm24(uint32_t addr)
{
    int32_t simm24 = RX_Read24(addr);
    simm24 = simm24 << 8;
    simm24 = simm24 >> 8;
    return simm24;
}

static uint32_t
RX_ReadCR(unsigned int cr)
{
    uint32_t value;
    switch (cr) {
        case 0:                /* PSW */
            value = RX_REG_PSW;
            break;
        case 1:                /* PC, instructions own address */
            value = RX_REG_PC;
            break;
        case 2:                /* USP */
            value = RX_GET_REG_USP();
            break;
        case 3:                /* FPSW */
            value = RX_REG_FPSW;
            break;
        case 8:                /* BPSW */
            value = RX_REG_BPSW;
            break;
        case 9:                /* BPC */
            value = RX_REG_BPC;
            break;
        case 10:               /* ISP */
            value = RX_GET_REG_ISP();
            break;
        case 11:               /* FINTV */
            value = RX_REG_FINTV;
            break;
        case 12:               /* INTB */
            value = RX_REG_INTB;
            break;
        default:
            fprintf(stderr, "Reading nonexisting CR %d\n", cr);
            value = 0;
            break;
    }
    return value;
}

/**
 **********************************************************************
 * Write control register in Supervisor mode
 **********************************************************************
 */
static void
RX_WriteCRSM(uint32_t value, unsigned int cr)
{
    switch (cr) {
        case 0:                /* PSW */
            /* PM bit can not be modified in SM */
            RX_SET_REG_PSW(value & ~PSW_PM);
            break;
        case 2:                /* USP */
            RX_SET_REG_USP(value);
            break;
        case 3:                /* FPSW */
            RX_REG_FPSW = value;
            break;
        case 8:                /* BPSW */
            RX_REG_BPSW = value;
            break;
        case 9:                /* BPC */
            RX_REG_BPC = value;
            break;
        case 10:               /* ISP */
            RX_SET_REG_ISP(value);
            break;
        case 11:               /* FINTV */
            RX_REG_FINTV = value;
            fprintf(stderr, "***************** FINTV %08x\n", value);
            break;
        case 12:               /* INTB */
            RX_REG_INTB = value;
            break;

        case 1:                /* PC, reserved */
        default:
            fprintf(stderr, "CRSM reserved register selected\n");
            break;

    }
}

/**
 *******************************************************************
 * Write to the control register in user mode
 *******************************************************************
 */
static void
RX_WriteCRUM(uint32_t value, unsigned int cr)
{
    switch (cr) {
        case 0:                /* PSW */
            RX_REG_PSW = (RX_REG_PSW & ~UINT32_C(0xf)) | (value & 0xf);
            break;

        case 2:                /* USP */
            RX_SET_REG_USP(value);
            break;

        case 3:                /* FPSW */
            RX_REG_FPSW = value;
            break;

        case 8:                /* BPSW */
        case 9:                /* BPC */
        case 10:               /* ISP */
        case 11:               /* FINTV */
        case 12:               /* INTB */
            fprintf(stderr, "Write to cr %d ignored in usermode\n", cr);
            break;

        case 1:                /* PC, reserved */
        default:
            fprintf(stderr, "reserved\n");
            break;

    }
}

static bool cnd_map[256];

static inline bool
check_condition(unsigned int cnd)
{
    unsigned int index = ((RX_REG_PSW & 0xf) << 4) | cnd;
    return cnd_map[index];
}

static void
init_condition_map()
{
    unsigned int flags, j;
    unsigned int index;
    bool result;
    for (flags = 0; flags < 16; flags++) {
        for (j = 0; j < 16; j++) {
            index = (flags << 4) | j;
            result = false;
            bool Z = ! !(flags & PSW_Z);
            bool C = ! !(flags & PSW_C);
            bool S = ! !(flags & PSW_S);
            bool O = ! !(flags & PSW_O);
            switch (j) {
                case 0:
                    if (Z) {
                        result = true;
                    }
                    break;

                case 1:
                    if (!Z) {
                        result = true;
                    }
                    break;

                case 2:        /* BGEU BC */
                    if (C) {
                        result = true;
                    }
                    break;

                case 3:        /* BLTU BNC */
                    if (!C) {
                        result = true;
                    }
                    break;

                case 4:        /* BGTU */
                    if (C & !Z) {
                        result = true;
                    }
                    break;
                case 5:        /* BLEU */
                    if ((C & !Z) == 0) {
                        result = true;
                    }
                    break;
                case 6:        /* BPZ */
                    if (S == 0) {
                        result = true;
                    }
                    break;
                case 7:        /* BN */
                    if (S == 1) {
                        result = true;
                    }
                    break;
                case 8:        /* BGE */
                    if ((S ^ O) == 0) {
                        result = true;
                    }
                    break;
                case 9:        /* BLT */
                    if ((S ^ O) == 1) {
                        result = true;
                    }
                    break;
                case 10:       /* BGT */
                    if (((S ^ O) | Z) == 0) {
                        result = true;
                    }
                    break;
                case 11:       /* BLE */
                    if (((S ^ O) | Z) == 1) {
                        result = true;
                    }
                    break;
                case 12:       /* BO */
                    if (O == 1) {
                        result = true;
                    }
                    break;
                case 13:       /* BNO */
                    if (O == 0) {
                        result = true;
                    }
                    break;
                case 14:       /* BRA.B */
                    result = true;
                    break;

                case 15:       /* ???????????? */
                    result = false;
                    break;
            }
            cnd_map[index] = result;
        }
    }
}

/**
 *********************************************************************
 * \fn void rx_abs_dst(void)
 * (1) Takes the absolute value and places the result in dest.
 * Flags: ZSO
 * v0
 *********************************************************************
 */
static void
rx_abs_dst(void)
{
    uint32_t Rd;
    Rd = *INSTR->pRd;
    if (ISNEG(Rd)) {
        Rd = -Rd;
        *INSTR->pRd = Rd;
    }
    RX_REG_PSW &= ~(PSW_O | PSW_S | PSW_Z);
    if (unlikely(ISNEG(Rd))) {
        RX_REG_PSW |= PSW_O | PSW_S;
    } else if (unlikely(Rd == 0)) {
        RX_REG_PSW |= PSW_Z;
    }
    RX_REG_PC += 2;
}

void
rx_setup_abs_dst(void)
{
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_abs_dst;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_abs_src_dst(void)
 * (2) Takes the absolute value and places the result in dest.
 * Flags: ZSO
 * v0
 ********************************************************************
 */
static void
rx_abs_src_dst(void)
{
    uint32_t Rs, Rd;
    Rs = *INSTR->pRs;
    if (ISNEG(Rs)) {
        Rd = -Rs;
    } else {
        Rd = Rs;
    }
    *INSTR->pRd = Rd;
    RX_REG_PSW &= ~(PSW_O | PSW_S | PSW_Z);
    if (unlikely(ISNEG(Rd))) {
        RX_REG_PSW |= PSW_O | PSW_S;
    } else if (unlikely(Rd == 0)) {
        RX_REG_PSW |= PSW_Z;
    }
    RX_REG_PC += 3;
}

void
rx_setup_abs_src_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_abs_src_dst;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_adc_imm_dst(void)
 * Add with carry. 
 * Addressing mode (1): signed immediate 
 * Flags: CZSO, length 4 to 7 
 * v0
 *****************************************************************
 */
static void
rx_adc_imm32_dst(void)
{
    uint32_t Rd = *INSTR->pRd;
    uint32_t result;
    uint32_t Src;
    Src = RX_Read32(RX_REG_PC + 3);
    RX_REG_PC += 7;
    result = Rd + Src;
    if (RX_REG_PSW & PSW_C) {
        result++;
    }
    *INSTR->pRd = result;
    add_flags(Rd, Src, result);
}

static void
rx_adc_simm8_dst(void)
{
    uint32_t Rd;
    uint32_t result;
    uint32_t Src;
    Src = RX_ReadSimm8(RX_REG_PC + 3);
    Rd = *INSTR->pRd;
    result = Rd + Src;
    RX_REG_PC += 4;
    if (RX_REG_PSW & PSW_C) {
        result++;
    }
    *INSTR->pRd = result;
    add_flags(Rd, Src, result);
}

static void
rx_adc_simm16_dst(void)
{
    uint32_t Rd;
    uint32_t result;
    uint32_t Src;
    Src = RX_ReadSimm16(RX_REG_PC + 3);
    Rd = *INSTR->pRd;
    result = Rd + Src;
    RX_REG_PC += 5;
    if (RX_REG_PSW & PSW_C) {
        result++;
    }
    *INSTR->pRd = result;
    add_flags(Rd, Src, result);
}

static void
rx_adc_simm24_dst(void)
{
    uint32_t Rd;
    uint32_t result;
    uint32_t Src;
    Src = RX_ReadSimm24(RX_REG_PC + 3);
    Rd = *INSTR->pRd;
    result = Rd + Src;
    RX_REG_PC += 6;
    if (RX_REG_PSW & PSW_C) {
        result++;
    }
    *INSTR->pRd = result;
    add_flags(Rd, Src, result);
}

void
rx_setup_adc_imm_dst(void)
{
    uint32_t rd = ICODE24() & 0xf;
    unsigned int li = (ICODE24() >> 10) & 0x3;
    INSTR->pRd = RX_RegP(rd);
    switch (li) {
        case 0:
            INSTR->proc = rx_adc_imm32_dst;
            break;
        case 1:
            INSTR->proc = rx_adc_simm8_dst;
            break;
        case 2:
            INSTR->proc = rx_adc_simm16_dst;
            break;
        default:
        case 3:
            INSTR->proc = rx_adc_simm24_dst;
            break;
    }
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_adc_src_dst(void)
 * Add with carry. 
 * Addressing mode (2): Rs + Rd 
 * Flags CZSO, length 3 bytes
 * v0
 *************************************************************
 */
static void
rx_adc_src_dst(void)
{
    uint32_t Rs, Rd;
    uint32_t result;
    Rs = *INSTR->pRs;
    Rd = *INSTR->pRd;
    result = Rd + Rs;
    if (RX_REG_PSW & PSW_C) {
        result++;
    }
    add_flags(Rd, Rs, result);
    *INSTR->pRd = result;
    RX_REG_PC += 3;
}

void
rx_setup_adc_src_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_adc_src_dst;
    INSTR->proc();
}

/**
 **********************************************************
 * \fn void rx_adc_isrc_dst(void)
 * Add with carry
 * addressing mode (3): indirect source
 * Flags: CSZO
 * v0
 **********************************************************
 */
void
rx_adc_isrc_dst(void)
{
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    unsigned int ld = (ICODE32() >> 16) & 3;
    uint32_t Src, Rs, Rd;
    uint32_t result;
    uint32_t dsp;
    Rs = RX_ReadReg(rs);
    Rd = RX_ReadReg(rd);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4) << 2;
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4) << 2;
            RX_REG_PC += 6;
            break;
        default:
            dsp = 0;
            fprintf(stderr, "Illegal ld in %s\n", __func__);
            break;
    }
    Src = RX_Read32(Rs + dsp);
    result = Rd + Src;
    if (RX_REG_PSW & PSW_C) {
        result++;
    }
    add_flags(Rd, Src, result);
    RX_WriteReg(result, rd);
}

/**
 *******************************************************************
 * \fn void rx_add_uimm_dst(void)
 * (1) Add a 4 bit unsigned immediate to a destination register.
 * Flags: CZSO, length 2 bytes.
 * v0
 *******************************************************************
 */
void
rx_add_uimm_dst(void)
{
    uint32_t uimm = INSTR->arg1;
    uint32_t Rd, result;
    Rd = *INSTR->pRd;
    result = Rd + uimm;
    *INSTR->pRd = result;
    add_flags(Rd, uimm, result);
    RX_REG_PC += 2;
}

void
rx_setup_add_uimm_dst(void)
{
    unsigned int rd = (ICODE16() & 0xf);
    uint32_t uimm = (ICODE16() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = uimm;
    INSTR->proc = rx_add_uimm_dst;
    INSTR->proc();
}

/**
 ****************************************************************
 * \fn void rx_add_src_dst_ub(void)
 * (2a) Add when UB or src = Rs 
 * v0
 ****************************************************************
 */
static void
rx_add_b_isrc_dst(void)
{
    uint32_t Rd, Src, result;
    Src = RX_Read8(*INSTR->pRs);
    RX_REG_PC += 2;
    Rd = *INSTR->pRd;
    result = Rd + Src;
    add_flags(Rd, Src, result);
    *INSTR->pRd = result;
}

static void
rx_add_b_isrc_dsp8_dst(void)
{
    uint32_t Rd, Src, result;
    uint32_t dsp;
    dsp = RX_Read8(RX_REG_PC + 2);
    Src = RX_Read8(*INSTR->pRs + dsp);
    RX_REG_PC += 3;
    Rd = *INSTR->pRd;
    result = Rd + Src;
    add_flags(Rd, Src, result);
    *INSTR->pRd = result;
}

static void
rx_add_b_isrc_dsp16_dst(void)
{
    uint32_t Rd, Src, result;
    uint32_t dsp;
    dsp = RX_Read16(RX_REG_PC + 2);
    Src = RX_Read8(*INSTR->pRs + dsp);
    RX_REG_PC += 4;
    Rd = *INSTR->pRd;
    result = Rd + Src;
    add_flags(Rd, Src, result);
    *INSTR->pRd = result;
}

static void
rx_add_rs_dst(void)
{
    uint32_t Rd, Src, result;
    Src = *INSTR->pRs;
    Rd = *INSTR->pRd;
    result = Rd + Src;
    RX_REG_PC += 2;
    add_flags(Rd, Src, result);
    *INSTR->pRd = result;
}

void
rx_setup_add_src_dst_ub(void)
{
    unsigned int ld = (ICODE16() >> 8) & 0x3;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int rd = (ICODE16() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    switch (ld) {
        case 0:
            INSTR->proc = rx_add_b_isrc_dst;
            break;
        case 1:
            INSTR->proc = rx_add_b_isrc_dsp8_dst;
            break;
        case 2:
            INSTR->proc = rx_add_b_isrc_dsp16_dst;
            break;
        default:
        case 3:
            INSTR->proc = rx_add_rs_dst;
            break;
    }
    INSTR->proc();
}

/**
 **************************************************************************
 * \fn void rx_add_src_dst_nub(void)
 * (2b) add B,W,L,UW to dst
 * v0
 **************************************************************************
 */
static void
rx_add_src_dst_nub(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int mi = INSTR->arg2;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = *INSTR->pRs;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            fprintf(stderr, "ADD: RS variant should not exist\n");
            RX_REG_PC += 3;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd + Src;
    *INSTR->pRd = result;
    add_flags(Rd, Src, result);
}

void
rx_setup_add_src_dst_nub(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int mi = (ICODE24() >> 14) & 0x3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->arg2 = mi;
    INSTR->proc = rx_add_src_dst_nub;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_add_simm_src2_dst(void)
 * (3) Add signed immediate src + src2 and write it to dst
 * length 3 to 6 bytes
 * v0
 *****************************************************************
 */
static void
rx_add_imm32_src2_dst(void)
{
    uint32_t Src2, Src, result;
    Src = RX_Read32(RX_REG_PC + 2);
    RX_REG_PC += 6;
    Src2 = *INSTR->pRs2;
    result = Src2 + Src;
    *INSTR->pRd = result;
    add_flags(Src, Src2, result);
}

static void
rx_add_simm8_src2_dst(void)
{
    uint32_t Src2, Src, result;
    Src = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 2);
    RX_REG_PC += 3;
    Src2 = *INSTR->pRs2;
    result = Src2 + Src;
    *INSTR->pRd = result;
    add_flags(Src, Src2, result);
}

static void
rx_add_simm16_src2_dst(void)
{
    uint32_t Src2, Src, result;
    Src = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 2);
    RX_REG_PC += 4;
    Src2 = *INSTR->pRs2;
    result = Src2 + Src;
    *INSTR->pRd = result;
    add_flags(Src, Src2, result);
}

static void
rx_add_simm24_src2_dst(void)
{
    uint32_t Src2, Src, result;
    Src = RX_Read24(RX_REG_PC + 2);
    Src = ((int32_t) (Src << 8)) >> 8;
    RX_REG_PC += 5;
    Src2 = *INSTR->pRs2;
    result = Src2 + Src;
    *INSTR->pRd = result;
    add_flags(Src, Src2, result);
}

void
rx_setup_add_simm_src2_dst(void)
{
    unsigned int rs2 = (ICODE16() >> 4) & 0xf;
    unsigned int rd = (ICODE16() & 0xf);
    unsigned int li = (ICODE16() >> 8) & 0x3;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->pRd = RX_RegP(rd);
    switch (li) {
        case 0:
            INSTR->proc = rx_add_imm32_src2_dst;
            break;
        case 1:
            INSTR->proc = rx_add_simm8_src2_dst;
            break;
        case 2:
            INSTR->proc = rx_add_simm16_src2_dst;
            break;
        default:
        case 3:
            INSTR->proc = rx_add_simm24_src2_dst;
            break;
    }
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_add_src_src2_dst(void)
 * (4) Add src1 to src2. Write to rd. 
 * v0
 ***************************************************************
 */
static void
rx_add_src_src2_dst(void)
{
    uint32_t Src, Src2, result;
    Src = *INSTR->pRs;
    Src2 = *INSTR->pRs2;
    result = Src + Src2;
    *INSTR->pRd = result;
    add_flags(Src, Src2, result);
    RX_REG_PC += 3;
}

void
rx_setup_add_src_src2_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = (ICODE24() & 0xf);
    unsigned int rd = (ICODE24() >> 8) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_add_src_src2_dst;
    INSTR->proc();
}

/*
 ****************************************************************
 * \fn void rx_and_uimm_dst(void)
 * (1) "and" of a four bit unsigned immediate and a register.
 * v0
 ****************************************************************
 */
static void
rx_and_uimm_dst(void)
{
    uint32_t uimm = INSTR->arg1;
    uint32_t Rd, result;
    Rd = *INSTR->pRd;
    result = Rd & uimm;
    *INSTR->pRd = result;
    RX_REG_PC += 2;
    and_flags(result);
}

void
rx_setup_and_uimm_dst(void)
{
    unsigned int rd = (ICODE16() & 0xf);
    uint32_t uimm = (ICODE16() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = uimm;
    INSTR->proc = rx_and_uimm_dst;
    INSTR->proc();
}

/**
 **************************************************************************
 * \fn void rx_and_imm_dst(void)
 * (2) And operation, signed immediate variant.
 * v0
 **************************************************************************
 */
static void
rx_and_imm32_dst(void)
{
    uint32_t Rd, result;
    uint32_t Src;
    Src = RX_Read32(RX_REG_PC + 2);
    RX_REG_PC += 6;
    Rd = *INSTR->pRd;
    result = Rd & Src;
    *INSTR->pRd = result;
    and_flags(result);
}

static void
rx_and_simm8_dst(void)
{
    uint32_t Rd, result;
    uint32_t Src;
    Src = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 2);
    RX_REG_PC += 3;
    Rd = *INSTR->pRd;
    result = Rd & Src;
    *INSTR->pRd = result;
    and_flags(result);
}

static void
rx_and_simm16_dst(void)
{
    uint32_t Rd, result;
    uint32_t Src;
    Src = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 2);
    RX_REG_PC += 4;
    Rd = *INSTR->pRd;
    result = Rd & Src;
    *INSTR->pRd = result;
    and_flags(result);
}

static void
rx_and_simm24_dst(void)
{
    uint32_t Rd, result;
    uint32_t Src;
    Src = RX_Read24(RX_REG_PC + 2);
    Src = ((int32_t) (Src << 8)) >> 8;
    RX_REG_PC += 5;
    Rd = *INSTR->pRd;
    result = Rd & Src;
    *INSTR->pRd = result;
    and_flags(result);
}

void
rx_setup_and_imm_dst(void)
{
    unsigned int rd = (ICODE16() & 0xf);
    unsigned int li = (ICODE16() >> 8) & 0x3;
    INSTR->pRd = RX_RegP(rd);
    switch (li) {
        case 0:
            INSTR->proc = rx_and_imm32_dst;
            break;
        case 1:
            INSTR->proc = rx_and_simm8_dst;
            break;
        case 2:
            INSTR->proc = rx_and_simm16_dst;
            break;
        default:
        case 3:
            INSTR->proc = rx_and_simm24_dst;
            break;
    }
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_and_src_dst_ub(void)
 * v0
 *******************************************************************
 */
static void
rx_and_src_dst_ub(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 2;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 3;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 2;
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd & Src;
    *INSTR->pRd = result;
    and_flags(result);
}

void
rx_setup_and_src_dst_ub(void)
{
    unsigned int ld = (ICODE16() >> 8) & 0x3;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int rd = (ICODE16() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_and_src_dst_ub;
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_and_src_dst_nub(void)
 * (4) add indirect src to a destination.
 * v0 
 ***************************************************************
 */
static void
rx_and_src_dst_nub(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int mi = INSTR->arg2;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = *INSTR->pRs;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            fprintf(stderr, "AND: RS variant should not exist\n");
            RX_REG_PC += 3;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd & Src;
    *INSTR->pRd = result;
    and_flags(result);
}

void
rx_setup_and_src_dst_nub(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int mi = (ICODE24() >> 14) & 0x3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->arg2 = mi;
    INSTR->proc = rx_and_src_dst_nub;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_and_src_src2_dst(void)
 * (4) And two registers and write to a third one.
 * v0
 ***********************************************************************
 */
static void
rx_and_src_src2_dst(void)
{
    uint32_t Src, Src2, result;
    Src = *INSTR->pRs;
    Src2 = *INSTR->pRs2;
    result = Src & Src2;
    *INSTR->pRd = result;
    and_flags(result);
    RX_REG_PC += 3;
}

void
rx_setup_and_src_src2_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = (ICODE24() & 0xf);
    unsigned int rd = (ICODE24() >> 8) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_and_src_src2_dst;
    INSTR->proc();
}

/**
 ********************************************************
 * \fn void rx_bclr_imm_dst(void)
 * Flags: no flag change
 * (1) imm7 version of Bit Clear. 
 * v0
 ********************************************************
 */
static void
rx_bclr_imm_dst(void)
{
    unsigned int bitNr = INSTR->arg1;
    unsigned int ld = INSTR->arg2;
    uint32_t addr = *INSTR->pRd;
    switch (ld) {
        case 0:
            RX_REG_PC += 2;
            break;
        case 1:
            addr += RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            addr += RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
        case 3:
            fprintf(stderr, "undef. amode in %s\n", __func__);
            return;
    }
    RX_Write8(RX_Read8(addr) & ~(1 << bitNr), addr);
}

void
rx_setup_bclr_imm_dst(void)
{
    unsigned int rd = (ICODE16() >> 4) & 0xf;
    unsigned int bitNr = ICODE16() & 0x7;
    unsigned int ld = (ICODE16() >> 8) & 0x3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = bitNr;
    INSTR->arg2 = ld;
    INSTR->proc = rx_bclr_imm_dst;
    INSTR->proc();
}

/**
 ***********************************************************
 * \fn void rx_bclr_rs_dst(void)
 * (2) Bit clear Rs indirect destination byte variant.
 * Flags: none
 * v0
 ***********************************************************
 */
static void
rx_bclr_rs_dst(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int bitNr;
    bitNr = *INSTR->pRs & 7;
    uint32_t addr;
    switch (ld) {
        case 0:
            addr = *INSTR->pRd;
            RX_REG_PC += 3;
            break;
        case 1:
            addr = *INSTR->pRd + RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            addr = *INSTR->pRd + RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            fprintf(stderr, "illegal amode in %s\n", __func__);
            return;
    }
    RX_Write8(RX_Read8(addr) & ~(1 << bitNr), addr);
}

void
rx_setup_bclr_rs_dst(void)
{
    unsigned int rs = ICODE24() & 0xf;
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_bclr_rs_dst;
    INSTR->proc();
}

/**
 **************************************************************
 * \fn void rx_bclr_imm5_dst(void)
 * (3) Clear a bit in a register
 * v0
 **************************************************************
 */
static void
rx_bclr_imm5_dst(void)
{
    unsigned int bitNr = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    Rd = Rd & ~(UINT32_C(1) << bitNr);
    *INSTR->pRd = Rd;
    RX_REG_PC += 2;
}

void
rx_setup_bclr_imm5_dst(void)
{
    unsigned int bitNr = (ICODE16() >> 4) & 0x1f;
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = bitNr;
    INSTR->proc = rx_bclr_imm5_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_bclr_src_dst(void)
 * (4) Clear a bit in a register with index from register.
 * v0
 *************************************************************
 */
static void
rx_bclr_src_dst(void)
{
    uint32_t Rd = *INSTR->pRd;
    uint32_t bitNr = *INSTR->pRs & 0x1f;
    Rd = Rd & ~(UINT32_C(1) << bitNr);
    *INSTR->pRd = Rd;
    RX_REG_PC += 3;
}

void
rx_setup_bclr_src_dst(void)
{
    unsigned int rs = ICODE24() & 0xf;
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_bclr_src_dst;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_bcnd_s_src(void)
 * (1) BEQ/BNE one byte
 * v0
 ********************************************************************
 */
static void
rx_beq_s_src(void)
{
    if (RX_REG_PSW & PSW_Z) {
        RX_REG_PC += INSTR->arg1;
    } else {
        RX_REG_PC += 1;
    }
}

void
rx_setup_beq_s_src(void)
{
    unsigned int dsp3 = ICODE8() & 7;
    INSTR->arg1 = (dsp3 < 3) ? (dsp3 | 8) : dsp3;
    INSTR->proc = rx_beq_s_src;
    INSTR->proc();
}

static void
rx_bne_s_src(void)
{
    if (!(RX_REG_PSW & PSW_Z)) {
        RX_REG_PC += INSTR->arg1;
    } else {
        RX_REG_PC += 1;
    }
}

void
rx_setup_bne_s_src(void)
{
    unsigned int dsp3 = ICODE8() & 7;
    unsigned int dist;
    dist = (dsp3 < 3) ? (dsp3 | 8) : dsp3;
    INSTR->arg1 = dist;
    INSTR->proc = rx_bne_s_src;
}

/**
 **************************************************************************
 * \fn void rx_bcnd_b_src(void)
 * Branch conditional with 8 bit signed pcdsp.
 * v0
 **************************************************************************
 */

void
rx_beq_b_src(void)
{
    if (RX_REG_PSW & PSW_Z) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bne_b_src(void)
{
    if (!(RX_REG_PSW & PSW_Z)) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bgeu_b_src(void)
{
    if (RX_REG_PSW & PSW_C) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bltu_b_src(void)
{
    if (!(RX_REG_PSW & PSW_C)) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bgtu_b_src(void)
{
    if ((RX_REG_PSW & PSW_C) && !(RX_REG_PSW & PSW_Z)) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bleu_b_src(void)
{
    if (!((RX_REG_PSW & PSW_C) && !(RX_REG_PSW & PSW_Z))) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bpz_b_src(void)
{
    if (!(RX_REG_PSW & PSW_S)) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bn_b_src(void)
{
    if (RX_REG_PSW & PSW_S) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bge_b_src(void)
{
    if (! !(RX_REG_PSW & PSW_S) == ! !(RX_REG_PSW & PSW_O)) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_blt_b_src(void)
{
    if (! !(RX_REG_PSW & PSW_S) != ! !(RX_REG_PSW & PSW_O)) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bgt_b_src(void)
{
    if (!((RX_REG_PSW & PSW_Z) || (! !(RX_REG_PSW & PSW_S) != ! !(RX_REG_PSW & PSW_O)))) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_ble_b_src(void)
{
    if ((RX_REG_PSW & PSW_Z) || (! !(RX_REG_PSW & PSW_S) != ! !(RX_REG_PSW & PSW_O))) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bo_b_src(void)
{
    if (RX_REG_PSW & PSW_O) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_bno_b_src(void)
{
    if (!(RX_REG_PSW & PSW_O)) {
        int8_t pcdsp8 = RX_Read8(RX_REG_PC + 1);
        RX_REG_PC += pcdsp8;
    } else {
        RX_REG_PC += 2;
    }
}

/**
 *****************************************************************
 * \fn void rx_bcnd_w_src(void)
 * BEQ/BNE with signed 16 bit displacement. 
 * v0
 *****************************************************************
 */
void
rx_beq_w_src(void)
{
    if (RX_REG_PSW & PSW_Z) {
        int16_t pcdsp16 = RX_Read16(RX_REG_PC + 1);
        RX_REG_PC += pcdsp16;
    } else {
        RX_REG_PC += 3;
    }
}

void
rx_bne_w_src(void)
{
    if (!(RX_REG_PSW & PSW_Z)) {
        int16_t pcdsp16 = RX_Read16(RX_REG_PC + 1);
        RX_REG_PC += pcdsp16;
    } else {
        RX_REG_PC += 3;
    }
}

/**
 *******************************************************************
 * \fn void rx_bmcnd_imm3_dst(void)
 * Bit modify depending on condition.
 * (1) imm3 indirect byte destination.
 * v0
 *******************************************************************
 */
static void
rx_bmcnd_imm3_dst(void)
{
    unsigned int cnd = INSTR->arg1;
    uint8_t imm3 = INSTR->arg2;
    unsigned int ld = INSTR->arg3;
    uint32_t Rd = *INSTR->pRd;
    uint32_t addr;
    uint8_t value;
    switch (ld) {
        case 0:
            addr = Rd;
            RX_REG_PC += 3;
            break;
        case 1:
            addr = Rd + RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            addr = Rd + RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
            fprintf(stderr, "Illegal ld in %s\n", __func__);
            return;
    }
    value = RX_Read8(addr);
    if (check_condition(cnd)) {
        value |= (1 << imm3);
    } else {
        value &= ~(1 << imm3);
    }
    RX_Write8(value, addr);
}

void
rx_setup_bmcnd_imm3_dst(void)
{
    unsigned int cnd = ICODE24() & 0xf;
    uint8_t imm3 = (ICODE24() >> 10) & 7;
    unsigned int ld = (ICODE24() >> 8) & 3;
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = cnd;
    INSTR->arg2 = imm3;
    INSTR->arg3 = ld;
    INSTR->proc = rx_bmcnd_imm3_dst;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_bmcnd_src_dst(void)
 * (2) Modify a bit in a register depending on a condition. 
 * v0 
 *****************************************************************
 */
static void
rx_bmcnd_src_dst(void)
{
    unsigned int cnd = INSTR->arg1;
    unsigned int imm5 = INSTR->arg2;
    uint32_t Rd = *INSTR->pRd;
    if (check_condition(cnd)) {
        Rd |= (UINT32_C(1) << imm5);
    } else {
        Rd &= ~(UINT32_C(1) << imm5);
    }
    *INSTR->pRd = Rd;
    RX_REG_PC += 3;
}

void
rx_setup_bmcnd_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int cnd = (ICODE24() > 4) & 0xf;
    unsigned int imm5 = (ICODE24() >> 8) & 31;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = cnd;
    INSTR->arg2 = imm5;
    INSTR->proc = rx_bmcnd_src_dst;
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_bnot_im3_dst(void)
 * (1) Invert a bit in an indirect addressed Byte 
 * Flags: none 
 * v0
 ***************************************************************
 */
static void
rx_bnot_im3_dst(void)
{
    uint8_t imm3 = INSTR->arg1;
    unsigned int ld = INSTR->arg2;
    uint32_t Rd = *INSTR->pRd;
    uint32_t addr;
    uint8_t value;
    switch (ld) {
        case 0:
            addr = Rd;
            RX_REG_PC += 3;
            break;
        case 1:
            addr = Rd + RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            addr = Rd + RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
            fprintf(stderr, "Illegal amode in %s\n", __func__);
            return;
    }
    value = RX_Read8(addr);
    value ^= (1 << imm3);
    RX_Write8(value, addr);
}

void
rx_setup_bnot_im3_dst(void)
{
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    uint8_t imm3 = (ICODE24() >> 10) & 7;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = imm3;
    INSTR->arg2 = ld;
    INSTR->proc = rx_bnot_im3_dst;
    INSTR->proc();
}

/**
 *************************************************************************
 * \fn void rx_bnot_rs_dst(void)
 * (2) Invert a bit in a indirect addressed byte with register source
 *
 *************************************************************************
 */
static void
rx_bnot_rs_dst(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int bit = *INSTR->pRs & 7;
    uint8_t value;
    uint32_t addr = *INSTR->pRd;
    switch (ld) {
        case 0:                /* [Rd] */
            RX_REG_PC += 3;
            break;
        case 1:                /* dsp:8[Rd] */
            addr += RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:                /* dsp:16[Rd] */
            addr += RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
            fprintf(stderr, "Illegal ld %d in %s\n", ld, __func__);
            break;
    }
    value = RX_Read8(addr);
    value ^= (1 << bit);
    RX_Write8(value, addr);
}

void
rx_setup_bnot_rs_dst(void)
{
    unsigned int rs = ICODE24() & 0xf;
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_bnot_rs_dst;
    INSTR->proc();
}

/**
 ****************************************************************
 * \fn void rx_bnot_imm5_rd(void)
 * (3) Invert a bit in a register (immediate bit nr.)
 * v0
 ****************************************************************
 */
void
rx_bnot_imm5_rd(void)
{
    unsigned int imm5 = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    Rd ^= (UINT32_C(1) << imm5);
    *INSTR->pRd = Rd;
    RX_REG_PC += 3;
}

void
rx_setup_bnot_imm5_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int imm5 = (ICODE24() >> 8) & 31;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = imm5;
    INSTR->proc = rx_bnot_imm5_rd;
    INSTR->proc();
}

/** 
 *****************************************************************
 * \fn void rx_bnot_rs_rd(void)
 * Invert a bit in a register addressed by a register. 
 * v0
 *****************************************************************
 */
static void
rx_bnot_rs_rd(void)
{
    uint32_t Src = *INSTR->pRs & 31;
    uint32_t Rd = *INSTR->pRd;
    Rd ^= (UINT32_C(1) << Src);
    *INSTR->pRd = Rd;
    RX_REG_PC += 3;
}

void
rx_setup_bnot_rs_rd(void)
{
    unsigned int rs = ICODE24() & 0xf;
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_bnot_rs_rd;
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_bra_s_src(void)
 * (1) Branch relative with a distance up to 10 bytes.
 * v0
 ***************************************************************
 */
static void
rx_bra_s_src(void)
{
    RX_REG_PC += INSTR->arg1;
}

void
rx_setup_bra_s_src(void)
{
    uint32_t dsp = ICODE8() & 7;
    uint32_t dist = (dsp < 3) ? (dsp | 8) : dsp;
    INSTR->arg1 = dist;
    INSTR->proc = rx_bra_s_src;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_bra_b_src(void)
 * (2) Branch relative with a signed 8 bit displacement
 * v0
 *****************************************************************
 */
void
rx_bra_b_src(void)
{
    int32_t dsp = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 1);
    RX_REG_PC += dsp;
}

/*
 ******************************************************************
 * \fn void rx_bra_w_src(void)
 * (3) Branch relative with a signed 16 bit displacement
 * v0
 ******************************************************************
 */
void
rx_bra_w_src(void)
{
    int32_t dsp = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 1);
    RX_REG_PC += dsp;
}

/**
 *************************************************************************
 * \fn void rx_bra_a_src(void)
 * (4) Branch relative with a signed 24 bit displacement.
 * v0
 *************************************************************************
 */
void
rx_bra_a_src(void)
{
    int32_t dsp = RX_Read24(RX_REG_PC + 1);
    dsp = (dsp << 8) >> 8;
    RX_REG_PC += dsp;
}

/**
 **********************************************************************
 * \fn void rx_bra_l_src(void)
 * (5) Branch relative with a 32 displacement from a register
 * v0
 **********************************************************************
 */
void
rx_bra_l_src(void)
{
    unsigned int rs = ICODE16() & 0xf;
    int32_t Rs = RX_ReadReg(rs);
    RX_REG_PC += Rs;
}

void
rx_brk(void)
{
    static CycleCounter_t last;
    fprintf(stderr, "rx_brk, diff %" PRIu64 "\n", CycleCounter_Get() - last);
    last = CycleCounter_Get();
    RX_REG_PC += 1;
    RX_Break();
    /* When not in debug mode this will exit */
    fprintf(stderr, "rx_brk not implemented\n");
    exit(1);
}

/**
 **********************************************************************
 * \fn void rx_bset_imm3_dst(void)
 * (1) Set a bit in an indirectly addressed byte.
 * v0
 **********************************************************************
 */
static void
rx_bset_imm3_dst(void)
{
    unsigned int imm3 = INSTR->arg1;
    unsigned int ld = INSTR->arg2;
    uint32_t Rd = *INSTR->pRd;
    uint32_t addr;
    uint8_t value;
    switch (ld) {
        case 0:
            addr = Rd;
            RX_REG_PC += 2;
            break;
        case 1:
            addr = Rd + RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            addr = Rd + RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
            fprintf(stderr, "illegal amode in %s\n", __func__);
            return;
    }
    value = RX_Read8(addr);
    value |= (1 << imm3);
    RX_Write8(value, addr);
}

void
rx_setup_bset_imm3_dst(void)
{
    unsigned int imm3 = ICODE16() & 7;
    unsigned int ld = (ICODE16() >> 8) & 3;
    unsigned int rd = (ICODE16() >> 4) & 0xf;
    INSTR->arg1 = imm3;
    INSTR->arg2 = ld;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_bset_imm3_dst;
    INSTR->proc();
}

/**
 *******************************************************************************
 * \fn void rx_bset_rs_dst(void)
 * Set a bit in indirect addressed memory with index from a register.
 * v0
 *******************************************************************************
 */
static void
rx_bset_rs_dst(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int Src = *INSTR->pRs & 7;
    uint32_t Rd = *INSTR->pRd;
    uint32_t addr;
    uint8_t value;
    switch (ld) {
        case 0:
            addr = Rd;
            RX_REG_PC += 3;
            break;
        case 1:
            addr = Rd + RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            addr = Rd + RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
            fprintf(stderr, "illegal amode in %s\n", __func__);
            return;
    }
    value = RX_Read8(addr);
    value |= (1 << Src);
    RX_Write8(value, addr);
}

void
rx_setup_bset_rs_dst(void)
{
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    unsigned int rs = ICODE24() & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_bset_rs_dst;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_bset_imm5_dst(void)
 * (3) Set a bit in a register
 * v0
 *******************************************************************
 */
static void
rx_bset_imm5_dst(void)
{
    unsigned int imm5 = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    Rd |= (UINT32_C(1) << imm5);
    *INSTR->pRd = Rd;
    RX_REG_PC += 2;
}

void
rx_setup_bset_imm5_dst(void)
{
    unsigned int rd = ICODE16() & 0xf;
    unsigned int imm5 = (ICODE16() >> 4) & 31;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = imm5;
    INSTR->proc = rx_bset_imm5_dst;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_bset_rs_rd(void)
 * (4) set a bit in a register with index from a register.
 * v0
 ********************************************************************
 */
static void
rx_bset_rs_rd(void)
{
    uint32_t Src = *INSTR->pRs & 31;
    uint32_t Rd = *INSTR->pRd;
    Rd |= (UINT32_C(1) << Src);
    *INSTR->pRd = Rd;
    RX_REG_PC += 3;
}

void
rx_setup_bset_rs_rd(void)
{
    unsigned int rs = ICODE24() & 0xf;
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_bset_rs_rd;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_bsr_w_src(void)
 * Branch to subroutine with 16 bit signed dsp 
 * v0
 ********************************************************************
 */
void
rx_bsr_w_src(void)
{
    int32_t dsp = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 1);
    uint32_t SP = RX_ReadReg(0);
    SP -= 4;
    RX_WriteReg(SP, 0);
    RX_Write32(RX_REG_PC + 3, SP);
    RX_REG_PC += dsp;
}

/**
 **********************************************************************
 * \fn void rx_bsr_a_src(void)
 * Branch to a subroutine 24 bit signed relative 
 * v0
 **********************************************************************
 */
void
rx_bsr_a_src(void)
{
    int32_t dsp = RX_Read24(RX_REG_PC + 1);
    uint32_t SP = RX_ReadReg(0);
    SP -= 4;
    RX_WriteReg(SP, 0);
    RX_Write32(RX_REG_PC + 4, SP);
    dsp = (dsp << 8) >> 8;
    RX_REG_PC += dsp;
}

/**
 ***********************************************************************
 * \fn void rx_bsr_l_src(void)
 * Branch to a subroutine with a 32 bit offset from a register.
 * v0
 ***********************************************************************
 */
static void
rx_bsr_l_src(void)
{
    int32_t Rs = *INSTR->pRs;
    uint32_t SP = RX_ReadReg(0);
    SP -= 4;
    RX_Write32(RX_REG_PC + 2, SP);
    RX_WriteReg(SP, 0);
    RX_REG_PC += Rs;
}

void
rx_setup_bsr_l_src(void)
{
    unsigned int rs = ICODE16() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_bsr_l_src;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_btst_imm3_src2(void)
 * Test if a bit is set. Set the result to the carry 
 * and the inverse result to the zero flag.
 * (1) indirect byte destination with immediate bit nr.
 * length 2 to 4 bytes.
 * v0
 *************************************************************
 */
static void
rx_btst_imm3_src2(void)
{
    unsigned int imm3 = INSTR->arg1;
    unsigned int ld = INSTR->arg2;
    uint32_t Rs = *INSTR->pRs;
    uint32_t addr;
    uint8_t value;
    switch (ld) {
        case 0:
            addr = Rs;
            RX_REG_PC += 2;
            break;
        case 1:
            addr = Rs + RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            addr = Rs + RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
            fprintf(stderr, "Illegal ld in %s\n", __func__);
            return;
    }
    value = RX_Read8(addr);
    if (value & (1 << imm3)) {
        RX_REG_PSW |= PSW_C;
        RX_REG_PSW &= ~PSW_Z;
    } else {
        RX_REG_PSW |= PSW_Z;
        RX_REG_PSW &= ~PSW_C;
    }
}

void
rx_setup_btst_imm3_src2(void)
{
    unsigned int imm3 = ICODE16() & 7;
    unsigned int ld = (ICODE16() >> 8) & 3;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = imm3;
    INSTR->arg2 = ld;
    INSTR->proc = rx_btst_imm3_src2;
    INSTR->proc();
}

/**
 ******************************************************************
 * \fn void rx_btst_rs_src2(void)
 * Test if a bit is set. Set the result to the carry 
 * and the inverse result to the zero flag.
 * (2) Bit number from a register and indirect source. 
 * length 3 to 5 bytes.
 * v0
 ******************************************************************
 */
static void
rx_btst_rs_src2(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rs2 = *INSTR->pRs2;
    uint32_t Rs = *INSTR->pRs;
    uint32_t addr;
    uint8_t value;
    switch (ld) {
        case 0:
            addr = Rs2;
            RX_REG_PC += 3;
            break;
        case 1:
            addr = Rs2 + RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            addr = Rs2 + RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
            fprintf(stderr, "illegal ld in %s\n", __func__);
            return;
    }
    value = RX_Read8(addr);
    if ((value >> Rs) & 1) {
        RX_REG_PSW |= PSW_C;
        RX_REG_PSW &= ~PSW_Z;
    } else {
        RX_REG_PSW |= PSW_Z;
        RX_REG_PSW &= ~PSW_C;
    }
}

void
rx_setup_btst_rs_src2(void)
{
    unsigned int rs2 = (ICODE24() >> 4) & 0xf;
    unsigned int rs = ICODE24() & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_btst_rs_src2;
    INSTR->proc();
}

/**
 ************************************************************
 * \fn void rx_btst_imm5_src2(void)
 * Test if a bit is set. Set the result to the carry 
 * and the inverse result to the zero flag.
 * (3) Immediate Bit number and register source 
 * length 2 bytes.
 * v0
 ************************************************************
 */
static void
rx_btst_imm5_src2(void)
{
    unsigned int imm5 = INSTR->arg1;
    uint32_t Rs = *INSTR->pRs;
    if ((Rs >> imm5) & 1) {
        RX_REG_PSW |= PSW_C;
        RX_REG_PSW &= ~PSW_Z;
    } else {
        RX_REG_PSW |= PSW_Z;
        RX_REG_PSW &= ~PSW_C;
    }
    RX_REG_PC += 2;
}

void
rx_setup_btst_imm5_src2(void)
{
    unsigned int rs = ICODE16() & 0xf;
    unsigned int imm5 = (ICODE16() >> 4) & 31;
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = imm5;
    INSTR->proc = rx_btst_imm5_src2;
    INSTR->proc();
}

/**
 ******************************************************************
 * \fn void rx_btst_rs_rs2(void)
 * Test if a bit is set. Set the result to the carry 
 * and the inverse result to the zero flag.
 * (4) Bit number and source are registers
 * v0 
 ******************************************************************
 */
static void
rx_btst_rs_rs2(void)
{
    uint32_t Src = *INSTR->pRs & 31;
    uint32_t Rs2 = *INSTR->pRs2;
    if ((Rs2 >> Src) & 1) {
        RX_REG_PSW |= PSW_C;
        RX_REG_PSW &= ~PSW_Z;
    } else {
        RX_REG_PSW |= PSW_Z;
        RX_REG_PSW &= ~PSW_C;
    }
    RX_REG_PC += 3;
}

void
rx_setup_btst_rs_rs2(void)
{
    unsigned int rs = ICODE24() & 0xf;
    unsigned int rs2 = (ICODE24() >> 4) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_btst_rs_rs2;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_clrpsw(void)
 * Clear a bit in the processor status word (CZSOIU)
 * length 2 bytes.
 * v0
 *************************************************************
 */
static void
rx_clrpsw(void)
{
    unsigned int cb = INSTR->arg1;
    RX_REG_PC += 2;
    if (cb < 4) {
        RX_REG_PSW &= ~(1 << cb);
    } else if (cb == 8) {
        if (!(RX_REG_PSW & PSW_PM)) {
            RX_SET_REG_PSW(RX_REG_PSW & ~PSW_I);
        } else {
            fprintf(stderr, "Warning: clearing PSW_I in usermode\n");
        }
    } else if (cb == 9) {
        if (!(RX_REG_PSW & PSW_PM)) {
            RX_SET_REG_PSW(RX_REG_PSW & ~PSW_U);
        } else {
            fprintf(stderr, "Warning: clearing PSW_U in usermode\n");
        }
    }
}

void
rx_setup_clrpsw(void)
{
    unsigned int cb = ICODE16() & 0xf;
    INSTR->arg1 = cb;
    INSTR->proc = rx_clrpsw;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_cmp_uimm4_rs(void)
 * compare by calculating src2 - src and setting the
 * flags. 
 * Flags: CZSO
 * (1) uimm4 src and Register src2
 *******************************************************************
 */
void
rx_cmp_uimm4_rs(void)
{
    uint32_t uimm = INSTR->arg1;
    uint32_t Rs2, result;
    Rs2 = *INSTR->pRs2;
    result = Rs2 - uimm;
    sub_flags(Rs2, uimm, result);
    RX_REG_PC += 2;
}

void
rx_setup_cmp_uimm4_rs(void)
{
    unsigned int rs2 = (ICODE16() & 0xf);
    uint32_t uimm = (ICODE16() >> 4) & 0xf;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->arg1 = uimm;
    INSTR->proc = rx_cmp_uimm4_rs;
    INSTR->proc();
}

/**
 ******************************************************************
 * \fn void rx_cmp_uimm8_rs(void)
 * (2) umm8 source and register Src2
 * length 3 bytes.
 * v0
 ******************************************************************
 */
static void
rx_cmp_uimm8_rs(void)
{
    uint32_t uimm = RX_Read8(RX_REG_PC + 2);
    uint32_t Rs2, result;
    Rs2 = *INSTR->pRs2;
    result = Rs2 - uimm;
    sub_flags(Rs2, uimm, result);
    RX_REG_PC += 3;
}

void
rx_setup_cmp_uimm8_rs(void)
{
    unsigned int rs2 = (ICODE16() & 0xf);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_cmp_uimm8_rs;
    INSTR->proc();
}

/**
 ************************************************************************
 * \fn void rx_cmp_imm_rs(void)
 * (3) signed immediate source and register src2
 * length 3 to 6
 * v0
 ************************************************************************
 */
static void
rx_cmp_simm8_rs(void)
{
    uint32_t Src2 = *INSTR->pRs2;
    uint32_t result;
    uint32_t Src;
    Src = RX_ReadSimm8(RX_REG_PC + 2);
    RX_REG_PC += 3;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

void
rx_setup_cmp_simm8_rs(void)
{
    uint32_t rs2 = ICODE16() & 0xf;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_cmp_simm8_rs;
    INSTR->proc();
}

static void
rx_cmp_simm16_rs(void)
{
    uint32_t Src2 = *INSTR->pRs2;
    uint32_t result;
    uint32_t Src;
    Src = RX_ReadSimm16(RX_REG_PC + 2);
    RX_REG_PC += 4;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

void
rx_setup_cmp_simm16_rs(void)
{
    uint32_t rs2 = ICODE16() & 0xf;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_cmp_simm16_rs;
    INSTR->proc();
}

static void
rx_cmp_simm24_rs(void)
{
    uint32_t Src2 = *INSTR->pRs2;
    uint32_t result;
    uint32_t Src;
    Src = RX_ReadSimm24(RX_REG_PC + 2);
    RX_REG_PC += 5;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

void
rx_setup_cmp_simm24_rs(void)
{
    uint32_t rs2 = ICODE16() & 0xf;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_cmp_simm24_rs;
    INSTR->proc();
}

static void
rx_cmp_imm32_rs(void)
{
    uint32_t Src2 = *INSTR->pRs2;
    uint32_t result;
    uint32_t Src;
    Src = RX_Read32(RX_REG_PC + 2);
    RX_REG_PC += 6;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

void
rx_setup_cmp_imm32_rs(void)
{
    uint32_t rs2 = ICODE16() & 0xf;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_cmp_imm32_rs;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_cmp_ubrs_src2(void)
 * Compare with indirect byte source with displacement or register source. 
 * Src2 is a Register.
 * Length 2 to 4 bytes
 * v0
 ***********************************************************************
 */

static void
rx_cmp_b_irs_src2(void)
{
    uint32_t Src2, Src, result;
    Src = RX_Read8(*INSTR->pRs);
    RX_REG_PC += 2;
    Src2 = *INSTR->pRs2;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

static void
rx_cmp_b_irs_dsp8_src2(void)
{
    uint32_t Src2, Src, result;
    uint32_t dsp;
    dsp = RX_Read8(RX_REG_PC + 2);
    Src = RX_Read8(*INSTR->pRs + dsp);
    RX_REG_PC += 3;
    Src2 = *INSTR->pRs2;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

static void
rx_cmp_b_irs_dsp16_src2(void)
{
    uint32_t Src2, Src, result;
    uint32_t dsp;
    dsp = RX_Read16(RX_REG_PC + 2);
    Src = RX_Read8(*INSTR->pRs + dsp);
    RX_REG_PC += 4;
    Src2 = *INSTR->pRs2;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

static void
rx_cmp_rs_src2(void)
{
    uint32_t Src2, Src, result;
    Src = *INSTR->pRs;
    RX_REG_PC += 2;
    Src2 = *INSTR->pRs2;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

void
rx_setup_cmp_ubrs_src2(void)
{
    unsigned int ld = (ICODE16() >> 8) & 0x3;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int rs2 = (ICODE16() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    switch (ld) {
        case 0:
            INSTR->proc = rx_cmp_b_irs_src2;
            break;
        case 1:
            INSTR->proc = rx_cmp_b_irs_dsp8_src2;
            break;
        case 2:
            INSTR->proc = rx_cmp_b_irs_dsp16_src2;
            break;
        default:
        case 3:
            INSTR->proc = rx_cmp_rs_src2;
            break;
    }
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_cmp_memex_src2(void)
 * Length 3 to 5 bytes.
 * v0
 *******************************************************************
 */
void
rx_cmp_memex_src2(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int mi = INSTR->arg2;
    uint32_t Src2, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = *INSTR->pRs;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            fprintf(stderr, "Illegal RS variant in %s\n", __func__);
            RX_REG_PC += 3;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Src2 = *INSTR->pRs2;
    result = Src2 - Src;
    sub_flags(Src2, Src, result);
}

void
rx_setup_cmp_memex_src2(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = (ICODE24() & 0xf);
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int mi = (ICODE24() >> 14) & 0x3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->arg1 = ld;
    INSTR->arg2 = mi;
    INSTR->proc = rx_cmp_memex_src2;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_div_imm_dst(void)
 * (1) Divide a destination by an immediate. 
 * v0
 *****************************************************************
 */
static void
rx_div_imm_dst(void)
{
    unsigned int li = INSTR->arg1;
    int32_t Rd = *INSTR->pRd;
    int32_t result;
    int32_t Src;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 3);
            RX_REG_PC += 7;
            break;
        case 1:
            Src = RX_ReadSimm8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            Src = RX_ReadSimm16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = RX_ReadSimm24(RX_REG_PC + 3);
            RX_REG_PC += 6;
            break;
    }
    if (Src != 0) {
        result = Rd / Src;
        RX_REG_PSW &= ~PSW_O;
    } else {
        /* 
         ***********************************************
         * Result is undefined, this is a guess, should
         * be verified with a real device 
         ***********************************************
         */
        result = Rd;
        RX_REG_PSW |= PSW_O;
    }
    if ((Rd == 0x80000000) && (Src == -1)) {
        RX_REG_PSW = PSW_O;
    }
    *INSTR->pRd = result;
}

void
rx_setup_div_imm_dst(void)
{
    uint32_t rd = ICODE24() & 0xf;
    unsigned int li = (ICODE24() >> 10) & 0x3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = li;
    INSTR->proc = rx_div_imm_dst;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_div_ubrs_dst(void)
 * Divide a destination register by Rs or an indirect unsigned
 * byte source
 *******************************************************************
 */
static void
rx_div_ubrs_dst(void)
{
    unsigned int ld = INSTR->arg1;
    int32_t Rd, Src, result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 3;
            break;
    }
    Rd = *INSTR->pRd;
    if (Src != 0) {
        result = Rd / Src;
        RX_REG_PSW &= ~PSW_O;
    } else {
        /* 
         ***********************************************
         * Result is undefined, this is a guess, should
         * be verified with a real device 
         ***********************************************
         */
        result = Rd;
        RX_REG_PSW |= PSW_O;
    }
#if 0
    source can not be - 1 because it is an unsigned byte if ((Rd == 0x80000000) && (Src == -1)) {
        RX_REG_PSW = PSW_O;
    }
#endif
    *INSTR->pRd = result;
}

void
rx_setup_div_ubrs_dst(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    INSTR->arg1 = ld;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_div_ubrs_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_div_memex_dst(void)
 * v0
 *************************************************************
 */
void
rx_div_memex_dst(void)
{
    unsigned int ld = (ICODE32() >> 16) & 0x3;
    unsigned int mi = (ICODE32() >> 22) & 0x3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    int32_t Rd, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "DIV: RS variant should not exist\n");
            RX_REG_PC += 4;
            return;

    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = RX_ReadReg(rd);
    if (Src != 0) {
        result = Rd / Src;
        RX_REG_PSW &= ~PSW_O;
    } else {
        /* 
         ***********************************************
         * Result is undefined, this is a guess, should
         * be verified with a real device 
         ***********************************************
         */
        result = Rd;
        RX_REG_PSW |= PSW_O;
    }
    if ((Rd == 0x80000000) && (Src == -1)) {
        RX_REG_PSW = PSW_O;
    }
    RX_WriteReg(result, rd);
}

/**
 *******************************************************
 * \fn void rx_divu_imm_dst(void)
 * Unsigned divide a register destination by a signed
 * immediate source. length four to  seven bytes. 
 * v0
 *******************************************************
 */
static void
rx_divu_imm_dst(void)
{
    unsigned int li = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    uint32_t result;
    uint32_t Src;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 3);
            RX_REG_PC += 7;
            break;
        case 1:
            Src = RX_ReadSimm8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            Src = RX_ReadSimm16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = RX_ReadSimm24(RX_REG_PC + 3);
            RX_REG_PC += 6;
            break;
    }
    if (Src != 0) {
        result = Rd / Src;
        RX_REG_PSW &= ~PSW_O;
    } else {
        /* 
         ***********************************************
         * Result is undefined, this is a guess, should
         * be verified with a real device 
         ***********************************************
         */
        result = Rd;
        RX_REG_PSW |= PSW_O;
    }
    *INSTR->pRd = result;
}

void
rx_setup_divu_imm_dst(void)
{
    uint32_t rd = ICODE24() & 0xf;
    unsigned int li = (ICODE24() >> 10) & 0x3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = li;
    INSTR->proc = rx_divu_imm_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_divu_ubrs_dst(void)
 * Unsigned divide of a destination register by a Register
 * src or an unsigned byte. 
 * v0
 *************************************************************
 */
static void
rx_divu_ubrs_dst(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 3;
            break;

    }
    Rd = *INSTR->pRd;
    if (Src != 0) {
        result = Rd / Src;
        RX_REG_PSW &= ~PSW_O;
    } else {
        /* 
         ***********************************************
         * Result is undefined, this is a guess, should
         * be verified with a real device 
         ***********************************************
         */
        result = Rd;
        RX_REG_PSW |= PSW_O;
    }
    *INSTR->pRd = result;
}

void
rx_setup_divu_ubrs_dst(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    INSTR->arg1 = ld;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_divu_ubrs_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_divu_memex_dst(void)
 * Unsigned divide of a destination register by an indirect
 * addressed byte, word, long, or unsigned word. 
 * v0
 *************************************************************
 */
void
rx_divu_memex_dst(void)
{
    unsigned int ld = (ICODE32() >> 16) & 0x3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    unsigned int mi = (ICODE32() >> 22) & 0x3;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "DIV: RS variant should not exist\n");
            RX_REG_PC += 4;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = RX_ReadReg(rd);
    if (Src != 0) {
        result = Rd / Src;
        RX_REG_PSW &= ~PSW_O;
    } else {
        /* 
         ***********************************************
         * Result is undefined, this is a guess, should
         * be verified with a real device 
         ***********************************************
         */
        result = Rd;
        RX_REG_PSW |= PSW_O;
    }
    RX_WriteReg(result, rd);
}

/**
 ********************************************************
 * Extended multiply signed
 * Two 32 bit operands are multiplied. The destination
 * is a register pair dest2:dest = dest*src
 * The src is a signed immediate.
 * v0
 ********************************************************
 */
static void
rx_emul_imm_dst(void)
{
    unsigned int li = INSTR->arg1;
    int32_t Rd = *INSTR->pRd;
    int64_t result;
    int32_t Src;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 3);
            RX_REG_PC += 7;
            break;
        case 1:
            Src = RX_ReadSimm8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            Src = RX_ReadSimm16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = RX_ReadSimm24(RX_REG_PC + 3);
            RX_REG_PC += 6;
            break;
    }
    result = (int64_t) Rd *(int64_t) Src;
    RX_WriteACC(result);
    *INSTR->pRd = result;
    *INSTR->pRs2 = result >> 32;
}

void
rx_setup_emul_imm_dst(void)
{
    uint32_t rd = ICODE24() & 0xf;
    unsigned int li = (ICODE24() >> 10) & 0x3;
    if (rd > 14) {
        fprintf(stderr, "Error, emul rd > 14\n");
        rx_und();
        return;
    }
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs2 = RX_RegP(rd + 1);
    INSTR->arg1 = li;
    INSTR->proc = rx_emul_imm_dst;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_emul_ubrs_dst(void)
 * multiply a destination by an indirectly addressed unsigned 
 * byte or a register.
 *******************************************************************
 */
static void
rx_emul_ubrs_dst(void)
{
    unsigned int ld = INSTR->arg1;
    int32_t Rd, Src;
    int64_t result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 3;
            break;

    }
    Rd = *INSTR->pRd;
    result = (int64_t) Rd *(int64_t) Src;
    RX_WriteACC(result);
    *INSTR->pRd = result;
    *INSTR->pRs2 = result >> 32;
}

void
rx_setup_emul_ubrs_dst(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    if (rd > 14) {
        fprintf(stderr, "Error: Illegal destination R15 in emul\n");
        rx_und();
        return;
    }
    INSTR->arg1 = ld;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs2 = RX_RegP(rd + 1);
    INSTR->proc = rx_emul_ubrs_dst;
    INSTR->proc();
}

/*
 *******************************************************************
 * \fn void rx_emul_memex_dst(void)
 * Multiply a destination register with an indirectly addressed
 * source with a displacement.
 * v0
 *******************************************************************
 */
void
rx_emul_memex_dst(void)
{
    unsigned int ld = (ICODE32() >> 16) & 0x3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    unsigned int mi = (ICODE32() >> 22) & 0x3;
    int32_t Rd, Src;
    int64_t result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "DIV: RS variant should not exist\n");
            RX_REG_PC += 4;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = RX_ReadReg(rd);
    result = (int64_t) Rd *(int64_t) Src;
    RX_WriteACC(result);
    RX_WriteReg(result, rd);
    if (rd <= 14) {
        RX_WriteReg(result >> 32, rd + 1);
    } else {
        fprintf(stderr, "emul: illegal destination R15\n");
    }
}

/**
 *****************************************************************
 * \fn void rx_emulu_imm_dst(void)
 * Unsigned multiply of a register with an unsigned sign extended   
 * immediate.
 * v0
 *****************************************************************
 */
static void
rx_emulu_imm_dst(void)
{
    unsigned int li = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    uint64_t result;
    uint32_t Src;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 3);
            RX_REG_PC += 7;
            break;
        case 1:
            Src = RX_ReadSimm8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            Src = RX_ReadSimm16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = RX_ReadSimm24(RX_REG_PC + 3);
            RX_REG_PC += 6;
            break;
    }
    result = (uint64_t) Rd *(uint64_t) Src;
    RX_WriteACC(result);
    *INSTR->pRd = result;
    *INSTR->pRs2 = result >> 32;
}

void
rx_setup_emulu_imm_dst(void)
{
    uint32_t rd = ICODE24() & 0xf;
    unsigned int li = (ICODE24() >> 10) & 0x3;
    if (rd > 14) {
        fprintf(stderr, "emulu: illegal destination R15\n");
        rx_und();
        return;
    }
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs2 = RX_RegP(rd + 1);
    INSTR->arg1 = li;
    INSTR->proc = rx_emulu_imm_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_emulu_ubrs_dst(void)
 * unsigned multiply a destination by an indirectly addressed 
 * unsigned byte or by a register. length 3 to 5
 * v0
 *************************************************************
 */
static void
rx_emulu_ubrs_dst(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rd, Src;
    uint64_t result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 3;
            break;
    }
    Rd = *INSTR->pRd;
    result = (uint64_t) Rd *(uint64_t) Src;
    RX_WriteACC(result);
    *INSTR->pRd = result;
    *INSTR->pRs2 = result >> 32;
}

void
rx_setup_emulu_ubrs_dst(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    if (rd > 14) {
        fprintf(stderr, "Illegal rd15 for emulu\n");
        rx_und();
        return;
    }
    INSTR->arg1 = ld;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs2 = RX_RegP(rd + 1);
    INSTR->proc = rx_emulu_ubrs_dst;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_emulu_memex_dst(void)
 * unsigned multiply of Rd with an indirectly addressed byte,
 * word,long, or unsigned word. 
 * v0
 *****************************************************************
 */
void
rx_emulu_memex_dst(void)
{
    unsigned int ld = (ICODE32() >> 16) & 0x3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    unsigned int mi = (ICODE32() >> 22) & 0x3;
    uint32_t Rd, Src;
    uint64_t result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "emulu: RS variant should not exist\n");
            RX_REG_PC += 4;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = RX_ReadReg(rd);
    result = (uint64_t) Rd *(uint64_t) Src;
    RX_WriteACC(result);
    RX_WriteReg(result, rd);
    if (rd <= 14) {
        RX_WriteReg(result >> 32, rd + 1);
    } else {
        fprintf(stderr, "emulu: Illegal destination R15\n");
    }
}

/**
 *******************************************************************
 * \fn void rx_fadd_imm32_rd(void)
 * Add a floating immediate to a destination register
 * v0
 *******************************************************************
 */
static void
rx_fadd_imm32_rd(void)
{
    Float32_t fSrc;
    Float32_t fRd;
    Float32_t fResult;
    fSrc = RX_CastToFloat32(RX_Read32(RX_REG_PC + 3));
    fRd = RX_CastToFloat32(*INSTR->pRd);
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fRd)) {
        RX_REG_FPSW |= FPSW_CE;
        fRd = fRd & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!RX_CheckForUnimplementedException()) {
        fResult = Float32_Add(RX_FLOAT_CONTEXT, fRd, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += 7;
            if (Float32_IsNegative(fResult)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Float32_IsPlusMinusNull(fResult)) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            *INSTR->pRd = RX_CastFromFloat32(fResult);
        }
    }
}

void
rx_setup_fadd_imm32_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_fadd_imm32_rd;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_fadd_src_dst(void)
 * Add a Register or an indirectly addressed source to
 * a destination.
 * v0
 *************************************************************
 */
static void
rx_fadd_src_dst(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t dsp;
    uint32_t Rs = *INSTR->pRs;
    uint32_t Rd;
    uint32_t dpc;
    Float32_t fSrc;
    Float32_t fRd;
    Float32_t fResult;
    fRd = RX_CastToFloat32(*INSTR->pRd);
    switch (ld) {
        case 0:
            fSrc = RX_CastToFloat32(RX_Read32(Rs));
            dpc = 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 5;
            break;
        default:
        case 3:
            fSrc = RX_CastToFloat32(Rs);
            dpc = 3;
            break;
    }
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fRd)) {
        RX_REG_FPSW |= FPSW_CE;
        fRd = fRd & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        fResult = Float32_Add(RX_FLOAT_CONTEXT, fRd, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += dpc;
            if (Float32_IsNegative(fResult)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Float32_IsPlusMinusNull(fResult)) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            Rd = RX_CastFromFloat32(fResult);
            *INSTR->pRd = Rd;
        }
    }
}

void
rx_setup_fadd_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_fadd_src_dst;
    INSTR->proc();
}

/**
 **********************************************************
 * \fn void rx_fcmp_imm32_rs2(void)
 * set the flags according to src2 - src.
 * v0
 **********************************************************
 */
static void
rx_fcmp_imm32_rs2(void)
{
    Float32_t fSrc;
    Float32_t fSrc2;
    int result;

    fSrc = RX_CastToFloat32(RX_Read32(RX_REG_PC + 3));
    fSrc2 = RX_CastToFloat32(*INSTR->pRs2);
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fSrc2)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc2 = fSrc2 & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        result = Float32_Cmp(RX_FLOAT_CONTEXT, fSrc2, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += 7;
            switch (result) {
                case SFC_LESS:
                    RX_REG_PSW |= PSW_S;
                    RX_REG_PSW &= ~(PSW_O | PSW_Z);
                    break;
                case SFC_EQUAL:
                    RX_REG_PSW |= PSW_Z;
                    RX_REG_PSW &= ~(PSW_O | PSW_S);
                    break;
                case SFC_GREATER:
                    RX_REG_PSW &= ~(PSW_O | PSW_S | PSW_Z);
                    break;
                case SFC_UNORDERD:
                    RX_REG_PSW |= PSW_O;
                    RX_REG_PSW &= ~(PSW_S | PSW_Z);
                    break;
                default:
                    fprintf(stderr, "Unexpexted comparison result %d\n", result);
                    break;
            }
        }
    }
}

void
rx_setup_fcmp_imm32_rs2(void)
{
    unsigned int rs2 = ICODE24() & 0xf;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_fcmp_imm32_rs2;
    INSTR->proc();
}

/**
 ************************************************************************
 * \fn void rx_fcmp_src_src2(void)
 * Floating point compare src2 - src
 * v0
 ************************************************************************
 */
static void
rx_fcmp_src_src2(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t dsp;
    uint32_t dpc;
    uint32_t Rs = *INSTR->pRs;
    Float32_t fSrc;
    Float32_t fSrc2;
    int result;
    fSrc2 = RX_CastToFloat32(*INSTR->pRs2);
    switch (ld) {
        case 0:
            fSrc = RX_CastToFloat32(RX_Read32(Rs));
            dpc = 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 5;
            break;
        default:
        case 3:
            fSrc = RX_CastToFloat32(Rs);
            dpc = 3;
            break;
    }
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fSrc2)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc2 = fSrc2 & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        result = Float32_Cmp(RX_FLOAT_CONTEXT, fSrc2, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += dpc;
            switch (result) {
                case SFC_LESS:
                    RX_REG_PSW |= PSW_S;
                    RX_REG_PSW &= ~(PSW_O | PSW_Z);
                    break;
                case SFC_EQUAL:
                    RX_REG_PSW |= PSW_Z;
                    RX_REG_PSW &= ~(PSW_O | PSW_S);
                    break;
                case SFC_GREATER:
                    RX_REG_PSW &= ~(PSW_O | PSW_S | PSW_Z);
                    break;
                case SFC_UNORDERD:
                    RX_REG_PSW |= PSW_O;
                    RX_REG_PSW &= ~(PSW_S | PSW_Z);
                    break;
                default:
                    fprintf(stderr, "Unexpexted comparison result %d\n", result);
                    break;
            }
        }
    }
}

void
rx_setup_fcmp_src_src2(void)
{
    unsigned int rs2 = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_fcmp_src_src2;
    INSTR->proc();
}

/**
 ****************************************************************
 * \fn void rx_fdiv_imm32_rd(void)
 * Floating point division of Rd by an immediate
 * v0
 ****************************************************************
 */
static void
rx_fdiv_imm32_rd(void)
{
    Float32_t fSrc;
    Float32_t fRd;
    Float32_t fResult;
    fSrc = RX_CastToFloat32(RX_Read32(RX_REG_PC + 3));
    fRd = RX_CastToFloat32(*INSTR->pRd);
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fRd)) {
        RX_REG_FPSW |= FPSW_CE;
        fRd = fRd & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        fResult = Float32_Div(RX_FLOAT_CONTEXT, fRd, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += 7;
            if (Float32_IsNegative(fResult)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Float32_IsPlusMinusNull(fResult)) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            *INSTR->pRd = RX_CastFromFloat32(fResult);
        }
    }
}

void
rx_setup_fdiv_imm32_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_fdiv_imm32_rd;
    INSTR->proc();
}

/**
 **************************************************************
 * \fn void rx_fdiv_src_dst(void)
 * Floating point division of dest by src
 * v0
 **************************************************************
 */
static void
rx_fdiv_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    uint32_t dsp;
    uint32_t dpc;
    uint32_t Rs = RX_ReadReg(rs);
    Float32_t fSrc;
    Float32_t fRd;
    Float32_t fResult;

    fRd = RX_CastToFloat32(RX_ReadReg(rd));
    switch (ld) {
        case 0:
            fSrc = RX_CastToFloat32(RX_Read32(Rs));
            dpc = 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 5;
            break;
        default:
        case 3:
            fSrc = RX_CastToFloat32(Rs);
            dpc = 3;
            break;
    }
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fRd)) {
        RX_REG_FPSW |= FPSW_CE;
        fRd = fRd & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        fResult = Float32_Div(RX_FLOAT_CONTEXT, fRd, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += dpc;
            if (Float32_IsNegative(fResult)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Float32_IsPlusMinusNull(fResult)) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            RX_WriteReg(RX_CastFromFloat32(fResult), rd);
        }
    }
}

void
rx_setup_fdiv_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_fdiv_src_dst;
}

/**
 *************************************************************
 * \fn void rx_fmul_imm32_rd(void)
 * Floating point multiplication of a destination with
 * an immediate.
 * v0
 *************************************************************
 */
static void
rx_fmul_imm32_rd(void)
{
    Float32_t fSrc;
    Float32_t fRd;
    Float32_t fResult;
    fSrc = RX_CastToFloat32(RX_Read32(RX_REG_PC + 3));
    fRd = RX_CastToFloat32(*INSTR->pRd);
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fRd)) {
        RX_REG_FPSW |= FPSW_CE;
        fRd = fRd & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        fResult = Float32_Mul(RX_FLOAT_CONTEXT, fRd, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += 7;
            if (Float32_IsNegative(fResult)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Float32_IsPlusMinusNull(fResult)) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            *INSTR->pRd = RX_CastFromFloat32(fResult);
        }
    }
}

void
rx_setup_fmul_imm32_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_fmul_imm32_rd;
    INSTR->proc();
}

/**
 **********************************************************
 * \fn void rx_fmul_src_dst(void)
 * Floating point multiply src with dst
 * v0
 **********************************************************
 */
static void
rx_fmul_src_dst(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t dsp;
    uint32_t dpc;
    uint32_t Rs = *INSTR->pRs;
    Float32_t fSrc;
    Float32_t fRd;
    Float32_t fResult;

    fRd = RX_CastToFloat32(*INSTR->pRd);
    switch (ld) {
        case 0:
            fSrc = RX_CastToFloat32(RX_Read32(Rs));
            dpc = 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 5;
            break;
        default:
        case 3:
            fSrc = RX_CastToFloat32(Rs);
            dpc = 3;
            break;
    }
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fRd)) {
        RX_REG_FPSW |= FPSW_CE;
        fRd = fRd & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        fResult = Float32_Mul(RX_FLOAT_CONTEXT, fRd, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += dpc;
            if (Float32_IsNegative(fResult)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Float32_IsPlusMinusNull(fResult)) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            *INSTR->pRd = RX_CastFromFloat32(fResult);
        }
    }
}

void
rx_setup_fmul_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_fmul_src_dst;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_fsub_imm32_dst(void)
 * floating point subtraction of dst - src. src is immediate
 * v0
 *****************************************************************
 */
static void
rx_fsub_imm32_dst(void)
{
    Float32_t fSrc;
    Float32_t fRd;
    Float32_t fResult;
    fSrc = RX_CastToFloat32(RX_Read32(RX_REG_PC + 3));
    fRd = RX_CastToFloat32(*INSTR->pRd);
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fRd)) {
        RX_REG_FPSW |= FPSW_CE;
        fRd = fRd & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        fResult = Float32_Sub(RX_FLOAT_CONTEXT, fRd, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += 7;
            if (Float32_IsNegative(fResult)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Float32_IsPlusMinusNull(fResult)) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            *INSTR->pRd = RX_CastFromFloat32(fResult);
        }
    }
}

void
rx_setup_fsub_imm32_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_fsub_imm32_dst;
    INSTR->proc();
}

/**
 ************************************************************
 * \fn void rx_fsub_src_dst(void)
 * subtract a src from a destination.
 * v0
 ************************************************************
 */
static void
rx_fsub_src_dst(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t dsp;
    uint32_t dpc;
    uint32_t Rs = *INSTR->pRs;
    Float32_t fSrc;
    Float32_t fRd;
    Float32_t fResult;

    fRd = RX_CastToFloat32(*INSTR->pRd);
    switch (ld) {
        case 0:
            fSrc = RX_CastToFloat32(RX_Read32(Rs));
            dpc = 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 5;
            break;
        default:
        case 3:
            fSrc = RX_CastToFloat32(Rs);
            dpc = 3;
            break;
    }
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fRd)) {
        RX_REG_FPSW |= FPSW_CE;
        fRd = fRd & 0x80000000;
    }
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = fSrc & 0x80000000;
    }
    if (!(RX_CheckForUnimplementedException())) {
        fResult = Float32_Sub(RX_FLOAT_CONTEXT, fRd, fSrc);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += dpc;
            if (Float32_IsNegative(fResult)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Float32_IsPlusMinusNull(fResult)) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            *INSTR->pRd = RX_CastFromFloat32(fResult);
        }
    }
}

void
rx_setup_fsub_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_fsub_src_dst;
    INSTR->proc();
}

/**
 ***********************************************************
 * \fn void rx_ftoi_src_dst(void)
 * Convert a floating point number to an integer.
 * v0
 ***********************************************************
 */
static void
rx_ftoi_src_dst(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t dsp;
    uint32_t dpc;
    uint32_t Rs = *INSTR->pRs;
    Float32_t fSrc;
    uint32_t Rd;

    switch (ld) {
        case 0:
            fSrc = RX_CastToFloat32(RX_Read32(Rs));
            dpc = 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            dpc = 5;
            break;
        default:
        case 3:
            fSrc = RX_CastToFloat32(Rs);
            dpc = 3;
            break;
    }
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    if (Float32_IsSubnormal(fSrc)) {
        RX_REG_FPSW |= FPSW_CE;
        fSrc = 0;               /* Integer does not discriminate +- 0 */
    }
    if (!(RX_CheckForUnimplementedException())) {
        Rd = Float32_ToInt32(RX_FLOAT_CONTEXT, fSrc, SFM_ROUND_ZERO);
        if (!RX_CheckForFloatingPointException()) {
            RX_REG_PC += dpc;
            if (ISNEG(Rd)) {
                RX_REG_PSW |= PSW_S;
            } else {
                RX_REG_PSW &= ~PSW_S;
            }
            if (Rd == 0) {
                RX_REG_PSW |= PSW_Z;
            } else {
                RX_REG_PSW &= ~PSW_Z;
            }
            *INSTR->pRd = Rd;
        }
    }
}

void
rx_setup_ftoi_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_ftoi_src_dst;
    INSTR->proc();
}

/**
 ***********************************************************
 * \fn void rx_int(void)
 * Generate a software interrupt.
 ***********************************************************
 */
void
rx_int(void)
{
    uint8_t imm = RX_Read8(RX_REG_PC + 2);
    RX_REG_PC += 3;
    RX_Trap(imm);
    fprintf(stderr, "rx_int not tested\n");
}

/**
 ****************************************************************
 * \fn void rx_itof_ubrs_rd(void)
 * Convert a byte or Rs integer to a floating point
 * v0
 ****************************************************************
 */
static void
rx_itof_ubrs_rd(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Src;
    uint32_t dsp;
    uint32_t dpc;
    Float32_t fRd;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            dpc = 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            dpc = 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            dpc = 5;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            dpc = 3;
            break;
    }
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    fRd = Float32_FromInt32(RX_FLOAT_CONTEXT, Src, SFM_ROUND_ZERO);
    if (!RX_CheckForFloatingPointException()) {
        RX_REG_PC += dpc;
        if (Float32_IsNegative(fRd)) {
            RX_REG_PSW |= PSW_S;
        } else {
            RX_REG_PSW &= ~PSW_S;
        }
        if (Float32_IsPlusMinusNull(fRd)) {
            RX_REG_PSW |= PSW_Z;
        } else {
            RX_REG_PSW &= ~PSW_Z;
        }
        *INSTR->pRd = RX_CastFromFloat32(fRd);
    }
}

void
rx_setup_itof_ubrs_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->proc = rx_itof_ubrs_rd;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_itof_src_dst(void)
 * Convert a unsigned byte or Register source to a floating 
 * point value.
 ********************************************************************
 */
void
rx_itof_src_dst(void)
{
    unsigned int ld = (ICODE32() >> 16) & 0x3;
    unsigned int mi = (ICODE32() >> 22) & 0x3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    uint32_t Src;
    uint32_t dsp;
    uint32_t dpc;
    Float32_t fRd;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            dpc = 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            dpc = 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            dpc = 6;
            break;

        default:
        case 3:
            fprintf(stderr, "ITOF: RS variant should not exist\n");
            dpc = 4;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    RX_REG_FPSW &= ~(FPSW_CV | FPSW_CO | FPSW_CZ | FPSW_CU | FPSW_CX | FPSW_CE);
    fRd = Float32_FromInt32(RX_FLOAT_CONTEXT, Src, SFM_ROUND_ZERO);
    if (!RX_CheckForFloatingPointException()) {
        RX_REG_PC += dpc;
        if (Float32_IsNegative(fRd)) {
            RX_REG_PSW |= PSW_S;
        } else {
            RX_REG_PSW &= ~PSW_S;
        }
        if (Float32_IsPlusMinusNull(fRd)) {
            RX_REG_PSW |= PSW_Z;
        } else {
            RX_REG_PSW &= ~PSW_Z;
        }
        RX_WriteReg(RX_CastFromFloat32(fRd), rd);
    }
}

/**
 ***************************************************************
 * \fn void rx_jmp_rs(void)
 * Jump to a register src
 * v0
 ***************************************************************
 */
static void
rx_jmp_rs(void)
{
    RX_REG_PC = *INSTR->pRs;
}

void
rx_setup_jmp_rs(void)
{
    unsigned int rs = ICODE16() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_jmp_rs;
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_jsr_rs(void)
 * Jump to a subroutine
 * v0
 ***************************************************************
 */
static void
rx_jsr_rs(void)
{
    uint32_t SP = RX_ReadReg(0);
    SP -= 4;
    RX_Write32(RX_REG_PC + 2, SP);
    RX_WriteReg(SP, 0);
    RX_REG_PC = *INSTR->pRs;
}

void
rx_setup_jsr_rs(void)
{
    unsigned int rs = ICODE16() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_jsr_rs;
    INSTR->proc();
}

/**
 ********************************************************
 * \fn void rx_machi(void)
 * Multiply and accumulate high order 
 * v0
 ********************************************************
 */
static void
rx_machi(void)
{
    int16_t Src, Src2;
    int32_t Prod;
    uint64_t Acc;
    Src = *INSTR->pRs >> 16;
    Src2 = *INSTR->pRs2 >> 16;
    Prod = (int32_t) Src *(int32_t) Src2;
    Acc = RX_ReadACC();
    Acc += ((int64_t) Prod) << 16;
    RX_WriteACC(Acc);
    RX_REG_PC += 3;
}

void
rx_setup_machi()
{
    unsigned int rs2 = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_machi;
    INSTR->proc();
}

/**
 ******************************************************
 * \fn void rx_maclo(void)
 * Multiply low order half of two registers and
 * add the sign extended value shifted by 16 bits to
 * the accumulator.
 * v0
 ******************************************************
 */
static void
rx_maclo(void)
{
    int16_t Src, Src2;
    int32_t Prod;
    uint64_t Acc;
    Src = *INSTR->pRs;
    Src2 = *INSTR->pRs2;
    Prod = (int32_t) Src *(int32_t) Src2;
    Acc = RX_ReadACC();
    Acc += ((int64_t) Prod) << 16;
    RX_WriteACC(Acc);
    RX_REG_PC += 3;
}

void
rx_setup_maclo()
{
    unsigned int rs2 = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_maclo;
    INSTR->proc();
}

/**
 *******************************************************
 * \fn void rx_max_simm_rd(void)
 * Write the maximum of Rd and a signed immediate to Rd.
 * It treats src and dest as signed values !
 * No flags are affected.
 * v0
 *******************************************************
 */
static void
rx_max_simm_rd(void)
{
    unsigned int li = INSTR->arg1;
    int32_t Dst = *INSTR->pRd;
    int32_t Src;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 3);
            RX_REG_PC += 7;
            break;
        case 1:
            Src = RX_ReadSimm8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            Src = RX_ReadSimm16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = RX_ReadSimm24(RX_REG_PC + 3);
            RX_REG_PC += 6;
            break;
    }
    if (Src > Dst) {
        *INSTR->pRd = Src;
    }
}

void
rx_setup_max_simm_rd(void)
{
    uint32_t rd = ICODE24() & 0xf;
    unsigned int li = (ICODE24() >> 10) & 0x3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = li;
    INSTR->proc = rx_max_simm_rd;
    INSTR->proc();
}

/**
 *********************************************************************
 * \fn void rx_max_srcub_dst(void)
 * Write the maximum of an unsigned byte or Register Src to a
 * destination.
 * No flags are affected.
 * v0
 *********************************************************************
 */
static void
rx_max_srcub_dst(void)
{
    unsigned int ld = INSTR->arg1;
    int32_t Dst, Src;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 3;
            break;
    }
    Dst = *INSTR->pRd;
    if (Src > Dst) {
        *INSTR->pRd = Src;
    }
}

void
rx_setup_max_srcub_dst(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_max_srcub_dst;
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_max_src_dst(void)
 * Write the maximum of an indirect src with displacement and
 * a Register to a Register destination.
 * length 4 to 6 bytes.
 * v0
 ***************************************************************
 */
void
rx_max_src_dst(void)
{
    unsigned int ld = (ICODE32() >> 16) & 0x3;
    unsigned int mi = (ICODE32() >> 22) & 3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    int32_t Dst, Src;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "MAX: RS variant should not exist\n");
            RX_REG_PC += 4;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Dst = RX_ReadReg(rd);
    if (Src > Dst) {
        RX_WriteReg(Src, rd);
    }
}

/**
 ***********************************************************************
 * \fn void rx_min_simm_rd(void)
 * compares src and destination as signed values and place the
 * smaller one in destination.
 * Immediate src variant with length 4 to 7 bytes.
 * v0
 ***********************************************************************
 */
static void
rx_min_simm_rd(void)
{
    unsigned int li = INSTR->arg1;
    int32_t Dst = *INSTR->pRd;
    int32_t Src;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 3);
            RX_REG_PC += 7;
            break;
        case 1:
            Src = RX_ReadSimm8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            Src = RX_ReadSimm16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            Src = RX_ReadSimm24(RX_REG_PC + 3);
            RX_REG_PC += 6;
            break;
    }
    if (Src < Dst) {
        *INSTR->pRd = Src;
    }
}

void
rx_setup_min_simm_rd(void)
{
    uint32_t rd = ICODE24() & 0xf;
    unsigned int li = (ICODE24() >> 10) & 0x3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = li;
    INSTR->proc = rx_min_simm_rd;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_min_srcub_rd(void)
 * Place the smallor one of a Register and an indirectly addressed
 * unisgned byte or register in a destination register.
 * length 3 to 6 bytes
 * v0
 ********************************************************************
 */
static void
rx_min_srcub_rd(void)
{
    unsigned int ld = INSTR->arg1;
    int32_t Dst, Src;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 3;
            break;
    }
    Dst = *INSTR->pRd;
    if (Src < Dst) {
        *INSTR->pRd = Src;
    }
}

void
rx_setup_min_srcub_rd(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_min_srcub_rd;
    INSTR->proc();
}

/**
 **********************************************************
 * \fn void rx_min_src_dst(void)
 * Place the smaller one of an indirectly addressed source
 * and a register destination in a register destination.
 * v0
 **********************************************************
 */
void
rx_min_src_dst(void)
{
    unsigned int ld = (ICODE32() >> 16) & 0x3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    unsigned int mi = (ICODE32() >> 22) & 3;
    int32_t Dst, Src;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "MIN: RS variant should not exist\n");
            RX_REG_PC += 4;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Dst = RX_ReadReg(rd);
    if (Src < Dst) {
        RX_WriteReg(Src, rd);
    }
}

/**
 ***************************************************************
 * \fn void rx_mov_rs_dsp5_rd(void)
 * Flags no change
 * (1) Move from a source register to a memory location with
 * an offset. 
 ***************************************************************
 */

static void
rx_mov_b_rs_dsp5_ird(void)
{
    uint32_t dsp5 = INSTR->arg1;
    uint32_t addr = *INSTR->pRd;    /* RX_ReadReg(rd);  */
    addr += dsp5;
    RX_Write8(*INSTR->pRs, addr);
    RX_REG_PC += 2;
}

static void
rx_mov_w_rs_dsp5_ird(void)
{
    uint32_t dsp5 = INSTR->arg1;
    uint32_t addr = *INSTR->pRd;    /* RX_ReadReg(rd);  */
    addr += dsp5 << 1;
    RX_Write16(*INSTR->pRs, addr);
    RX_REG_PC += 2;
}

static void
rx_mov_l_rs_dsp5_ird(void)
{
    uint32_t dsp5 = INSTR->arg1;
    uint32_t addr = *INSTR->pRd;    /* RX_ReadReg(rd);  */
    addr += dsp5 << 2;
    RX_Write32(*INSTR->pRs, addr);
    RX_REG_PC += 2;
}

void
rx_setup_mov_rs_dsp5_ird(void)
{
    uint32_t dsp5 = ((ICODE16() >> 3) & 1) | ((ICODE16() >> 6) & 0x1e);
    unsigned int rd = (ICODE16() >> 4) & 7;
    unsigned int rs = (ICODE16() & 7);
    unsigned int sz = (ICODE16() >> 12) & 3;
    INSTR->arg1 = dsp5;
    INSTR->arg2 = sz;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    switch (sz) {
        case 0:
            INSTR->proc = rx_mov_b_rs_dsp5_ird;
            break;
        case 1:
            INSTR->proc = rx_mov_w_rs_dsp5_ird;
            break;
        default:
        case 2:
            INSTR->proc = rx_mov_l_rs_dsp5_ird;
            break;
    }
    INSTR->proc();
}

/**
 ****************************************************************
 * \fn void rx_mov_dsp5_rs_rd(void)
 * (2) Move from a memory location with 5 bit displacement 
 * to a register. B and W are sign extended
 * v0
 ****************************************************************
 */

static void
rx_mov_b_dsp5_irs_rd(void)
{
    uint32_t dsp5 = INSTR->arg1;
    uint32_t addr = *INSTR->pRs;
    int32_t value;
    addr += dsp5;
    value = (int32_t) (int8_t) RX_Read8(addr);
    *INSTR->pRd = value;
    RX_REG_PC += 2;
}

static void
rx_mov_w_dsp5_irs_rd(void)
{
    uint32_t dsp5 = INSTR->arg1;
    uint32_t addr = *INSTR->pRs;
    int32_t value;
    addr += dsp5 << 1;
    value = (int32_t) (int16_t) RX_Read16(addr);
    *INSTR->pRd = value;
    RX_REG_PC += 2;
}

static void
rx_mov_l_dsp5_irs_rd(void)
{
    uint32_t dsp5 = INSTR->arg1;
    uint32_t addr = *INSTR->pRs;
    addr += dsp5 << 2;
    *INSTR->pRd = RX_Read32(addr);
    RX_REG_PC += 2;
}

void
rx_setup_mov_dsp5_irs_rd(void)
{
    uint32_t dsp5 = ((ICODE16() >> 3) & 1) | ((ICODE16() >> 6) & 0x1e);
    int rs = (ICODE16() >> 4) & 7;
    int rd = (ICODE16() & 7);
    int sz = (ICODE16() >> 12) & 3;
    INSTR->arg1 = dsp5;
    INSTR->arg2 = sz;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    switch (sz) {
        case 0:
            INSTR->proc = rx_mov_b_dsp5_irs_rd;
            break;
        case 1:
            INSTR->proc = rx_mov_w_dsp5_irs_rd;
            break;
        default:
        case 2:
            INSTR->proc = rx_mov_l_dsp5_irs_rd;
            break;
    }
    INSTR->proc();
}

/**
 ******************************************************************
 * \fn void rx_mov_uimm4_rd(void)
 * (3) Mov a 4 bit unsigned immediate to a register 
 * v0
 ******************************************************************
 */
void
rx_mov_uimm4_rd(void)
{
    uint32_t uimm4 = INSTR->arg1;
    *INSTR->pRd = uimm4;
    RX_REG_PC += 2;
}

void
rx_setup_mov_uimm4_rd(void)
{
    uint32_t uimm4 = (ICODE16() >> 4) & 0xf;
    unsigned int rd = (ICODE16() & 0xf);
    INSTR->proc = rx_mov_uimm4_rd;
    INSTR->arg1 = uimm4;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc();
}

/**
 **************************************************************************
 * \fn void rx_mov_imm8_dsp5_rd(void)
 * (4) move a 8 bit unsigned immediate to a destination register 
 * with displacement. 
 **************************************************************************
 */
void
rx_mov_imm8_dsp5_rd(void)
{
    uint32_t dsp5 = INSTR->arg1;
    unsigned int sz = INSTR->arg2;
    uint32_t addr = *INSTR->pRd;
    uint8_t uimm8 = RX_Read8(RX_REG_PC + 2);
    switch (sz) {
        case 0:
            addr += dsp5;
            RX_Write8(uimm8, addr);
            break;
        case 1:
            addr += dsp5 << 1;
            RX_Write16(uimm8, addr);
            break;
        case 2:
            addr += dsp5 << 2;
            RX_Write32(uimm8, addr);
            break;
        default:
            fprintf(stderr, "Unknown size  %d in %s\n", sz, __func__);
            break;
    }
    RX_REG_PC += 3;
}

void
rx_setup_mov_imm8_dsp5_rd(void)
{
    unsigned int rd = (ICODE16() >> 4) & 0x7;
    unsigned int sz = (ICODE16() >> 8) & 3;
    uint32_t dsp5 = (ICODE16() & 0xf) | ((ICODE16() >> 3) & 0x10);
    INSTR->proc = rx_mov_imm8_dsp5_rd;
    INSTR->arg1 = dsp5;
    INSTR->arg2 = sz;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc();
}

/**
 **************************************************************
 * \fn void rx_mov_uimm8_rd(void)
 * (5) move an eight bit unsigned immediate to a register.
 **************************************************************
 */
static void
rx_mov_uimm8_rd(void)
{
    uint8_t uimm8 = RX_Read8(RX_REG_PC + 2);
    *INSTR->pRd = uimm8;
    RX_REG_PC += 3;
}

void
rx_setup_mov_uimm8_rd(void)
{
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_mov_uimm8_rd;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_mov_simm_rd(void)
 * (6) Move a signed immediate to a destination register.
 * v0 
 *****************************************************************
 */
static void
rx_mov_imm32_rd(void)
{
    int32_t imm;
    imm = RX_Read32(RX_REG_PC + 2);
    *INSTR->pRd = imm;
    RX_REG_PC += 6;
}

static void
rx_mov_simm8_rd(void)
{
    int32_t imm;
    imm = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 2);
    *INSTR->pRd = imm;
    RX_REG_PC += 3;
}

static void
rx_mov_simm16_rd(void)
{
    int32_t imm;
    imm = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 2);
    *INSTR->pRd = imm;
    RX_REG_PC += 4;
}

static void
rx_mov_simm24_rd(void)
{
    int32_t imm;
    imm = RX_Read24(RX_REG_PC + 2);
    imm = (imm << 8) >> 8;
    *INSTR->pRd = imm;
    RX_REG_PC += 5;
}

void
rx_setup_mov_simm_rd(void)
{
    unsigned int rd = (ICODE16() >> 4) & 0xf;
    unsigned int li = (ICODE16() >> 2) & 3;
    INSTR->arg1 = li;
    INSTR->pRd = RX_RegP(rd);
    switch (li) {
        case 0:
            INSTR->proc = rx_mov_imm32_rd;
            break;
        case 1:
            INSTR->proc = rx_mov_simm8_rd;
            break;
        case 2:
            INSTR->proc = rx_mov_simm16_rd;
            break;
        default:
        case 3:
            INSTR->proc = rx_mov_simm24_rd;
            break;
    }
    INSTR->proc();
}

/**
 ****************************************************************************
 * \fn void rx_mov_rs_rd(void)
 * (7) Move a register src to a register destination. Sign extend
 * to 32 Bit. 
 * v0
 ****************************************************************************
 */
static void
rx_mov_rs_rd(void)
{
    unsigned int sz = INSTR->arg1;
    uint32_t value;
    switch (sz) {
        case 0:
            value = (int32_t) (int8_t) * INSTR->pRs;
            break;
        case 1:
            value = (int32_t) (int16_t) * INSTR->pRs;
            break;
        case 2:
            value = *INSTR->pRs;
            break;
        default:
            fprintf(stderr, "Illegal size %d in %s\n", sz, __func__);
            return;
    }
    *INSTR->pRd = value;
    RX_REG_PC += 2;
}

void
rx_setup_mov_rs_rd(void)
{
    unsigned int rd = ICODE16() & 0xf;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int sz = (ICODE16() >> 12) & 0x3;
    INSTR->proc = rx_mov_rs_rd;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = sz;
    INSTR->proc();
}

/**
 *****************************************************************
 * (8) MOV.size src,dest
 * move a signed immediate to a indirectly addressed destination
 * with displacement. This currently includes some nonexsiting 
 * opcodes.
 * v0
 *****************************************************************
 */
static void
rx_mov_simm_dsp_ird(void)
{
    unsigned int sz = INSTR->arg1;
    unsigned int li = INSTR->arg2;
    unsigned int ld = INSTR->arg3;
    uint32_t addr = *INSTR->pRd;
    uint32_t dsp;
    int32_t imm32;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 2;
            break;

        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
            fprintf(stderr, "Unknown ld in opcode\n");
            return;
    }
    switch (li) {
        case 0:
            imm32 = RX_Read32(RX_REG_PC);
            RX_REG_PC += 4;
            break;
        case 1:
            imm32 = (int32_t) (int8_t) RX_Read8(RX_REG_PC);
            RX_REG_PC += 1;
            break;

        case 2:
            imm32 = (int32_t) (int16_t) RX_Read16(RX_REG_PC);
            RX_REG_PC += 2;
            break;

        case 3:
            imm32 = RX_Read24(RX_REG_PC);
            imm32 = (imm32 << 8) >> 8;
            RX_REG_PC += 3;
            break;

        default:
            fprintf(stderr, "Unknown li in opcode\n");
            return;
    }
    switch (sz) {
        case 0:
            addr = addr + dsp;
            RX_Write8(imm32, addr);
            break;
        case 1:
            addr = addr + (dsp << 1);
            RX_Write16(imm32, addr);
            break;
        case 2:
            addr = addr + (dsp << 2);
            RX_Write32(imm32, addr);
            break;
        default:
            fprintf(stderr, "Unknown sz %d in %s\n", sz, __func__);
            return;
    }
}

void
rx_setup_mov_simm_dsp_ird(void)
{
    unsigned int rd = (ICODE16() >> 4) & 0xf;
    unsigned int sz = ICODE16() & 3;
    unsigned int li = (ICODE16() >> 2) & 3;
    unsigned int ld = (ICODE16() >> 8) & 3;
    INSTR->proc = rx_mov_simm_dsp_ird;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = sz;
    INSTR->arg2 = li;
    INSTR->arg3 = ld;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_mov_dsp_irs_rd(void)
 * (9) Move an indirect src with displacement to a destination 
 * register.
 * v0 
 *****************************************************************
 */
static void
rx_mov_dsp_irs_rd(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int sz = INSTR->arg2;
    uint32_t addr = *INSTR->pRs;
    uint32_t dsp;
    uint32_t value;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 2;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
            fprintf(stderr, "Illegal ld in instruction\n");
            return;
    }
    switch (sz) {
        case 0:
            addr = addr + dsp;
            value = (int32_t) (int8_t) RX_Read8(addr);
            break;
        case 1:
            addr = addr + (dsp << 1);
            value = (int32_t) (int16_t) RX_Read16(addr);
            break;
        case 2:
            addr = addr + (dsp << 2);
            value = RX_Read32(addr);
            break;
        default:
            fprintf(stderr, "Illegal sz in instruction\n");
            return;
    }
    //fprintf(stderr,"R%02u from %08x is %08x\n",rd,addr,value);
    *INSTR->pRd = value;
}

void
rx_setup_mov_dsp_irs_rd(void)
{

    unsigned int rd = ICODE16() & 0xf;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int ld = (ICODE16() >> 8) & 0x3;
    unsigned int sz = (ICODE16() >> 12) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->arg2 = sz;
    INSTR->proc = rx_mov_dsp_irs_rd;
    INSTR->proc();
}

/**
 *******************************************************************************
 * void rx_mov_irirb_rd(void)
 * (10) move from an address built from two registers to a destination
 * register. 
 * v0
 *******************************************************************************
 */
static void
rx_mov_irirb_rd(void)
{
    uint32_t *Rd = INSTR->pRd;
    uint32_t *Rb = INSTR->pRs;
    uint32_t *Ri = INSTR->pRs2;
    unsigned int sz = INSTR->arg1;
    uint32_t addr;
    uint32_t value;
    addr = *Rb;
    switch (sz) {
        case 0:
            addr += *Ri;
            value = (int32_t) (int8_t) RX_Read8(addr);
            break;
        case 1:
            addr += *Ri << 1;
            value = (int32_t) (int16_t) RX_Read16(addr);
            break;
        case 2:
            addr += *Ri << 2;
            value = RX_Read32(addr);
            break;
        default:
            return;
    }
    RX_REG_PC += 3;
    *Rd = value;
}

void
rx_setup_mov_irirb_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rb = (ICODE24() >> 4) & 0xf;
    unsigned int ri = (ICODE24() >> 8) & 0xf;
    unsigned int sz = (ICODE24() >> 12) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rb);
    INSTR->pRs2 = RX_RegP(ri);
    INSTR->arg1 = sz;
    INSTR->proc = rx_mov_irirb_rd;
    INSTR->proc();
}

/**
 *********************************************************************
 * \fn void rx_mov_rs_dsp_ird(void)
 * (11) move from a source register to an indirect destination with
 * offset.
 * v0
 *********************************************************************
 */
static void
rx_mov_rs_dsp_ird(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int sz = INSTR->arg2;
    uint32_t dsp;
    uint32_t addr = *INSTR->pRd;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 2;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
            fprintf(stderr, "Illegal ld in instruction\n");
            return;

    }
    switch (sz) {
        case 0:
            addr = addr + dsp;
            RX_Write8(*INSTR->pRs, addr);
            break;
        case 1:
            addr = addr + (dsp << 1);
            RX_Write16(*INSTR->pRs, addr);
            break;
        case 2:
            addr = addr + (dsp << 2);
            RX_Write32(*INSTR->pRs, addr);
            break;
        default:
            fprintf(stderr, "Illegal sz in instruction\n");
            return;
    }
}

void
rx_setup_mov_rs_dsp_ird(void)
{
    unsigned int rs = ICODE16() & 0xf;
    unsigned int rd = (ICODE16() >> 4) & 0xf;
    unsigned int ld = (ICODE16() >> 10) & 3;
    unsigned int sz = (ICODE16() >> 12) & 3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->arg2 = sz;
    INSTR->proc = rx_mov_rs_dsp_ird;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_mov_rs_irird(void)
 * (12) move from a register to a memory location addressed by
 * two registers 
 * v0
 *******************************************************************
 */
static void
rx_mov_rs_irird(void)
{
    uint32_t Rs = *INSTR->pRs;
    uint32_t Rb = *INSTR->pRd;
    uint32_t Ri = *INSTR->pRs2;
    unsigned int sz = INSTR->arg1;
    uint32_t addr = Rb;
    uint32_t value = Rs;
    switch (sz) {
        case 0:
            addr += Ri;
            RX_Write8(value, addr);
            break;
        case 1:
            addr += Ri << 1;
            RX_Write16(value, addr);
            break;
        case 2:
            addr += Ri << 2;
            RX_Write32(value, addr);
            break;
        default:
            fprintf(stderr, "Illegal sz in opcode\n");
            break;
    }
    RX_REG_PC += 3;
}

void
rx_setup_mov_rs_irird(void)
{
    unsigned int rs = (ICODE24() & 0xf);
    unsigned int rb = (ICODE24() >> 4) & 0xf;
    unsigned int ri = (ICODE24() >> 8) & 0xf;
    unsigned int sz = (ICODE24() >> 12) & 0x3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rb);
    INSTR->pRs2 = RX_RegP(ri);
    INSTR->arg1 = sz;
    INSTR->proc = rx_mov_rs_irird;
    INSTR->proc();
}

/**
 ************************************************************************
 * (13) MOV.size src,dest
 * move from an indirect src with displacement to an indirect destination
 * with displacement
 * v0
 ************************************************************************
 */
static void
rx_mov_dsp_irs_dsp_ird(void)
{
    unsigned int ldd = INSTR->arg1;
    unsigned int lds = INSTR->arg2;
    unsigned int sz = INSTR->arg3;
    uint32_t daddr, saddr;
    uint32_t dsps;
    uint32_t dspd;
    switch (lds) {
        case 0:
            dsps = 0;
            RX_REG_PC += 2;
            break;
        case 1:
            dsps = RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            dsps = RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
            fprintf(stderr, "illegal lds in opcode\n");
            return;
    }
    switch (ldd) {
        case 0:
            dspd = 0;
            break;
        case 1:
            dspd = RX_Read8(RX_REG_PC);
            RX_REG_PC += 1;
            break;
        case 2:
            dspd = RX_Read16(RX_REG_PC);
            RX_REG_PC += 2;
            break;
        default:
            fprintf(stderr, "illegal ldd in opcode\n");
            return;
    }
    saddr = *INSTR->pRs;
    daddr = *INSTR->pRd;
    switch (sz) {
        case 0:
            saddr += dsps;
            daddr += dspd;
            RX_Write8(RX_Read8(saddr), daddr);
            break;
        case 1:
            saddr += (dsps << 1);
            daddr += (dspd << 1);
            RX_Write16(RX_Read16(saddr), daddr);
            break;
        case 2:
            saddr += (dsps << 2);
            daddr += (dspd << 2);
            RX_Write32(RX_Read32(saddr), daddr);
            break;
        default:
            return;
    }
}

void
rx_setup_mov_dsp_irs_dsp_ird(void)
{
    unsigned int rd = ICODE16() & 0xf;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int ldd = (ICODE16() >> 10) & 3;
    unsigned int lds = (ICODE16() >> 8) & 3;
    unsigned int sz = (ICODE16() >> 12) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ldd;
    INSTR->arg2 = lds;
    INSTR->arg3 = sz;
    INSTR->proc = rx_mov_dsp_irs_dsp_ird;
    INSTR->proc();
}

/**
 ************************************************************************
 * \fn void rx_mov_rs_irdpm(void)
 * (14) mov to an indirect addressed destination with postincrement
 * or predecrement. In case of rs == rd the value before updating
 * is transfered as source.
 * v0
 ************************************************************************
 */
static void
rx_mov_rs_irdpm(void)
{
    unsigned int ad = INSTR->arg1;
    unsigned int sz = INSTR->arg2;
    unsigned int daddr;
    daddr = *INSTR->pRd;;
    switch (sz) {
        case 0:
            if (ad == 1) {
                /* Pre decrement */
                daddr -= 1;
            }
            RX_Write8(*INSTR->pRs, daddr);
            if (ad == 0) {
                /* Post increment */
                daddr += 1;
            }
            *INSTR->pRd = daddr;
            break;
        case 1:
            if (ad == 1) {
                /* Pre decrement */
                daddr -= 2;
            }
            RX_Write16(*INSTR->pRs, daddr);
            if (ad == 0) {
                /* Post increment */
                daddr += 2;
            }
            *INSTR->pRd = daddr;
            break;
        case 2:
            if (ad == 1) {
                /* Pre decrement */
                daddr -= 4;
            }
            RX_Write32(*INSTR->pRs, daddr);
            if (ad == 0) {
                /* Post increment */
                daddr += 4;
            }
            *INSTR->pRd = daddr;
            break;

        default:
            fprintf(stderr, "Illegal sz in %s\n", __func__);
            return;
    }
    RX_REG_PC += 3;
}

void
rx_setup_mov_rs_irdpm(void)
{
    unsigned int rs = ICODE24() & 0xf;
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    unsigned int ad = (ICODE24() >> 10) & 3;
    unsigned int sz = (ICODE24() >> 8) & 3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ad;
    INSTR->arg2 = sz;
    INSTR->proc = rx_mov_rs_irdpm;
    INSTR->proc();
}

/**
 *************************************************************************
 * \đn void rx_mov_irspm_rd(void)
 * (15) Move from a indirect source to a destination register. With
 * predecrement or postincrement of source register 
 * In case of rs == rd the data from memory location is saved in Rd.
 * v0
 *************************************************************************
 */
static void
rx_mov_irspm_rd(void)
{
    unsigned int ad = INSTR->arg1;
    unsigned int sz = INSTR->arg2;
    uint32_t saddr;
    saddr = *INSTR->pRs;
    switch (sz) {
        case 0:
            if (ad == 3) {
                /* Pre decrement */
                saddr -= 1;
                *INSTR->pRs = saddr;
            } else if (ad == 2) {
                *INSTR->pRs = saddr + 1;
            }
            *INSTR->pRd = (int32_t) (int8_t) RX_Read8(saddr);
            break;
        case 1:
            if (ad == 3) {
                /* Pre decrement */
                saddr -= 2;
                *INSTR->pRs = saddr;
            } else if (ad == 2) {
                *INSTR->pRs = saddr + 2;
            }
            *INSTR->pRd = (int32_t) (int16_t) RX_Read16(saddr);
            break;
        case 2:
            if (ad == 3) {
                /* Pre decrement */
                saddr -= 4;
                *INSTR->pRs = saddr;
            } else if (ad == 2) {
                *INSTR->pRs = saddr + 4;
            }
            *INSTR->pRd = RX_Read32(saddr);
            break;
        default:
            fprintf(stderr, "Illegal sz in %s\n", __func__);
            break;
    }
    RX_REG_PC += 3;
}

void
rx_setup_mov_irspm_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ad = (ICODE24() >> 10) & 3;
    unsigned int sz = (ICODE24() >> 8) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ad;
    INSTR->arg2 = sz;
    INSTR->proc = rx_mov_irspm_rd;
    INSTR->proc();
}

/**
 **********************************************************
 * \fn void rx_movu_dsp5_rs_rd(void)
 * (1) unsigned move a indirect source with displacement
 * to a destination register. 
 * Length is 2 bytes.
 * v0
 **********************************************************
 */
static void
rx_movu_b_dsp5_irs_rd(void)
{
    uint32_t dsp5 = INSTR->arg1;
    uint32_t addr = *INSTR->pRs;
    uint32_t value;
    addr += dsp5;
    value = RX_Read8(addr);
    *INSTR->pRd = value;
    RX_REG_PC += 2;
}

static void
rx_movu_w_dsp5_irs_rd(void)
{
    uint32_t dsp5 = INSTR->arg1;
    uint32_t addr = *INSTR->pRs;
    uint32_t value;
    addr += dsp5 << 1;
    value = RX_Read16(addr);
    *INSTR->pRd = value;
    RX_REG_PC += 2;
}

void
rx_setup_movu_dsp5_irs_rd(void)
{
    unsigned int rd = ICODE16() & 0x7;
    unsigned int rs = (ICODE16() >> 4) & 7;
    uint32_t dsp5 = ((ICODE16() >> 3) & 1) | ((ICODE16() >> 6) & 0x1e);
    int sz = (ICODE16() >> 11) & 1;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = dsp5;
    switch (sz) {
        case 0:
            INSTR->proc = rx_movu_b_dsp5_irs_rd;
            break;
        case 1:
            INSTR->proc = rx_movu_w_dsp5_irs_rd;
            break;
    }
    INSTR->proc();
}

/**
 **************************************************************
 * \fn void rx_movu_src_rd(void)
 * (2) unsigned move an indirect addressed source or register
 * to a register.
 * v0
 **************************************************************
 */
static void
rx_movu_b_isrc_rd(void)
{
    uint32_t saddr, Rs;
    Rs = *INSTR->pRs;
    RX_REG_PC += 2;
    saddr = Rs;
    *INSTR->pRd = RX_Read8(saddr);
}

static void
rx_movu_w_isrc_rd(void)
{
    uint32_t saddr, Rs;
    Rs = *INSTR->pRs;
    RX_REG_PC += 2;
    saddr = Rs;
    *INSTR->pRd = RX_Read16(saddr);
}

static void
rx_movu_b_dsp8_isrc_rd(void)
{
    uint32_t saddr, Rs;
    uint32_t dsp;
    Rs = *INSTR->pRs;
    dsp = RX_Read8(RX_REG_PC + 2);
    RX_REG_PC += 3;
    saddr = Rs + dsp;
    *INSTR->pRd = RX_Read8(saddr);
}

static void
rx_movu_w_dsp8_isrc_rd(void)
{
    uint32_t saddr, Rs;
    uint32_t dsp;
    Rs = *INSTR->pRs;
    dsp = RX_Read8(RX_REG_PC + 2);
    RX_REG_PC += 3;
    saddr = Rs + (dsp << 1);
    *INSTR->pRd = RX_Read16(saddr);
}

static void
rx_movu_b_dsp16_isrc_rd(void)
{
    uint32_t saddr, Rs;
    uint32_t dsp;
    Rs = *INSTR->pRs;
    dsp = RX_Read16(RX_REG_PC + 2);
    RX_REG_PC += 4;
    saddr = Rs + dsp;
    *INSTR->pRd = RX_Read8(saddr);
}

static void
rx_movu_w_dsp16_isrc_rd(void)
{
    uint32_t saddr, Rs;
    uint32_t dsp;
    Rs = *INSTR->pRs;
    dsp = RX_Read16(RX_REG_PC + 2);
    RX_REG_PC += 4;
    saddr = Rs + (dsp << 1);
    *INSTR->pRd = RX_Read16(saddr);
}

static void
rx_movu_b_rs_rd(void)
{
    uint32_t Rs;
    Rs = *INSTR->pRs;
    RX_REG_PC += 2;
    *INSTR->pRd = Rs & 0xff;
}

static void
rx_movu_w_rs_rd(void)
{
    uint32_t Rs;
    Rs = *INSTR->pRs;
    RX_REG_PC += 2;
    *INSTR->pRd = Rs & 0xffff;
}

void
rx_setup_movu_src_rd(void)
{
    unsigned int rd = ICODE16() & 0xf;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int ld = (ICODE16() >> 8) & 3;
    unsigned int sz = (ICODE16() >> 10) & 1;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    switch (ld) {
        case 0:
            if (sz) {
                INSTR->proc = rx_movu_w_isrc_rd;
            } else {
                INSTR->proc = rx_movu_b_isrc_rd;
            }
            break;
        case 1:
            if (sz) {
                INSTR->proc = rx_movu_w_dsp8_isrc_rd;
            } else {
                INSTR->proc = rx_movu_b_dsp8_isrc_rd;
            }
            break;
        case 2:
            if (sz) {
                INSTR->proc = rx_movu_w_dsp16_isrc_rd;
            } else {
                INSTR->proc = rx_movu_b_dsp16_isrc_rd;
            }
            break;
        case 3:
            if (sz) {
                INSTR->proc = rx_movu_w_rs_rd;
            } else {
                INSTR->proc = rx_movu_b_rs_rd;
            }
            break;
    }
    INSTR->proc();
}

/*
 ******************************************************************
 * \fn void rx_movu_irirb_rd(void)
 * (3) Unsigned move from a source addressed by two registers to
 * a destination register 
 * v0
 ******************************************************************
 */
static void
rx_movu_b_irirb_rd(void)
{
    uint32_t Rb = *INSTR->pRs;
    uint32_t saddr;
    uint32_t Ri = *INSTR->pRs2;
    saddr = Rb + Ri;;
    *INSTR->pRd = RX_Read8(saddr);
    RX_REG_PC += 3;
}

static void
rx_movu_w_irirb_rd(void)
{
    uint32_t Rb = *INSTR->pRs;
    uint32_t saddr;
    uint32_t Ri = *INSTR->pRs2;
    saddr = Rb + (Ri << 1);
    *INSTR->pRd = RX_Read16(saddr);
    RX_REG_PC += 3;
}

void
rx_setup_movu_irirb_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rb = (ICODE24() >> 4) & 0xf;
    unsigned int ri = (ICODE24() >> 8) & 0xf;
    unsigned int sz = (ICODE24() >> 12) & 1;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rb);
    INSTR->pRs2 = RX_RegP(ri);
    if (sz) {
        INSTR->proc = rx_movu_w_irirb_rd;
    } else {
        INSTR->proc = rx_movu_b_irirb_rd;
    }
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_movu_rspm_rd(void)
 * (4) unsigned move from a source addressed by a predecremented
 * or postincremented register to a destination Register 
 * If Rs == Rd the value from memory is transfered to Rd.
 * v0
 ***************************************************************
 */

static void
rx_movu_rspm_rd(void)
{
    unsigned int ad = INSTR->arg1;
    unsigned int sz = INSTR->arg2;
    uint32_t saddr;
    saddr = *INSTR->pRs;
    switch (sz) {
        case 0:
            if (ad == 3) {
                /* Pre decrement */
                saddr -= 1;
                *INSTR->pRs = saddr;
            } else if (ad == 2) {
                *INSTR->pRs = saddr + 1;
            }
            *INSTR->pRd = RX_Read8(saddr);
            break;

        default:
        case 1:
            if (ad == 3) {
                /* Pre decrement */
                saddr -= 2;
                *INSTR->pRs = saddr;
            } else if (ad == 2) {
                *INSTR->pRs = saddr + 2;
            }
            *INSTR->pRd = RX_Read16(saddr);
            break;
    }
    RX_REG_PC += 3;
}

void
rx_setup_movu_rspm_rd(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ad = (ICODE24() >> 10) & 3;
    unsigned int sz = (ICODE24() >> 8) & 1;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ad;
    INSTR->arg2 = sz;
    INSTR->proc = rx_movu_rspm_rd;
    INSTR->proc();
}

/**
 **************************************************************
 * \fn void rx_mul_uimm4_rd(void)
 * Multiply with unsigned 4 bit immediate. 
 * v0
 **************************************************************
 */
static void
rx_mul_uimm4_rd(void)
{
    uint32_t uimm4 = INSTR->arg1;
    uint32_t value = *INSTR->pRd;
    value *= uimm4;
    *INSTR->pRd = value;
    RX_REG_PC += 2;
}

void
rx_setup_mul_uimm4_rd(void)
{
    unsigned int rd = ICODE16() & 0xf;
    uint32_t uimm4 = (ICODE16() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = uimm4;
    INSTR->proc = rx_mul_uimm4_rd;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_mul_simm_rd(void)
 * Multiply with a sign extended immediate.
 * size is 3 to 6 bytes.
 * v0
 *******************************************************************
 */
static void
rx_mul_simm_rd(void)
{
    unsigned int li = INSTR->arg1;
    int32_t Src;
    int32_t Dst;
    Dst = *INSTR->pRd;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 2);
            RX_REG_PC += 6;
            break;
        case 1:
            Src = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            Src = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
        case 3:
            Src = RX_Read24(RX_REG_PC + 2);
            Src = (Src << 8) >> 8;
            RX_REG_PC += 5;
            break;
    }
    Dst *= Src;
    *INSTR->pRd = Dst;
}

void
rx_setup_mul_simm_rd(void)
{
    unsigned int li = (ICODE16() >> 8) & 0x3;
    unsigned int rd = ICODE16() & 0xf;
    INSTR->arg1 = li;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_mul_simm_rd;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_mul_srcub_rd(void)
 * Multiply with an indirectly addressed unsigned byte or a register 
 * v0
 *******************************************************************
 */
static void
rx_mul_srcub_rd(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 2;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 3;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 2;
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd * Src;
    *INSTR->pRd = result;
}

void
rx_setup_mul_srcub_rd(void)
{
    unsigned int ld = (ICODE16() >> 8) & 0x3;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int rd = (ICODE16() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_mul_srcub_rd;
    INSTR->proc();
}

/**
 *********************************************************************
 * \fn void rx_mul_src_rd(void)
 * Multiply a destination register with an indirectly addressed source
 * with displacement
 * v0
 *********************************************************************
 */
static void
rx_mul_src_rd(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int mi = INSTR->arg2;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = *INSTR->pRs;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            fprintf(stderr, "MUL: RS variant should not exist\n");
            RX_REG_PC += 3;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd * Src;
    *INSTR->pRd = result;
}

void
rx_setup_mul_src_rd(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int mi = (ICODE24() >> 14) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->arg2 = mi;
    INSTR->proc = rx_mul_src_rd;
    INSTR->proc();
}

/**
 *******************************************************
 * Multiply src 1 with src2 and write the result to 
 * a third register. 
 * v0
 *******************************************************
 */
static void
rx_mul_rsrs2_rd(void)
{
    uint32_t Src, Src2, result;
    Src = *INSTR->pRs;
    Src2 = *INSTR->pRs2;
    result = Src * Src2;
    RX_REG_PC += 3;
    *INSTR->pRd = result;
}

void
rx_setup_mul_rsrs2_rd(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = (ICODE24() & 0xf);
    unsigned int rd = (ICODE24() >> 8) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_mul_rsrs2_rd;
    INSTR->proc();
}

/**
 *********************************************************************
 * \fn void rx_mulhi_rs_rs2(void)
 * Mulhi multiplies the high order 16 Bits of rs and rs2 and stores
 * the sign extended result in the accumulator. The lower 16 Bit of
 * the ACC are cleared.
 * v0
 *********************************************************************
 */
static void
rx_mulhi_rs_rs2(void)
{
    int32_t Rs, Rs2;
    int64_t result;
    Rs = *INSTR->pRs;
    Rs2 = *INSTR->pRs2;
    result = (int64_t) ((Rs >> 16) * (Rs2 >> 16));
    result <<= 16;
    RX_WriteACC(result);
    RX_REG_PC += 3;
}

void
rx_setup_mulhi_rs_rs2(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = ICODE24() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_mulhi_rs_rs2;
    INSTR->proc();
}

/**
 ******************************************************************
 * \fn void rx_mullo_rs_rs2(void)
 * The low order 16 bits of rs and rs2 are multiplied.
 * The result is stored in the accumulator after sign extension
 * and left shift by 16 bits
 * v0
 ******************************************************************
 */
static void
rx_mullo_rs_rs2(void)
{
    int32_t Rs, Rs2;
    int64_t result;
    Rs = *INSTR->pRs;
    Rs2 = *INSTR->pRs2;
    result = (int64_t) ((int32_t) (int16_t) Rs * (int32_t) (int16_t) Rs2);
    result <<= 16;
    RX_WriteACC(result);
    RX_REG_PC += 3;
}

void
rx_setup_mullo_rs_rs2(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = ICODE24() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_mullo_rs_rs2;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_mvfachi(void)
 * Move the high order long word from the acc to a destination
 * register
 *****************************************************************
 */
static void
rx_mvfachi(void)
{
    uint64_t ACC;
    ACC = RX_ReadACC();
    *INSTR->pRd = ACC >> 32;
    RX_REG_PC += 3;
}

void
rx_setup_mvfachi(void)
{
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_mvfachi;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_mvfacmi(void)
 * Move the middle order long word from the accumulator to a 
 * register.
 * v0
 *******************************************************************
 */
static void
rx_mvfacmi(void)
{
    uint64_t ACC;
    ACC = RX_ReadACC();
    *INSTR->pRd = ACC >> 16;
    RX_REG_PC += 3;
}

void
rx_setup_mvfacmi(void)
{
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_mvfacmi;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_mvfc(void)
 * Move from control register.
 * v0
 *******************************************************************
 */
static void
rx_mvfc(void)
{
    unsigned int cr = INSTR->arg1;
    uint32_t value;
    value = RX_ReadCR(cr);
    *INSTR->pRd = value;
    RX_REG_PC += 3;
}

void
rx_setup_mvfc(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int cr = (ICODE24() >> 4) & 0xf;
    INSTR->proc = rx_mvfc;
    INSTR->arg1 = cr;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_mvtachi(void)
 * Move from source register to accumulator high
 * v0
 ********************************************************************
 */
static void
rx_mvtachi(void)
{
    uint64_t ACC;
    ACC = (uint32_t) RX_ReadACC();
    ACC = ACC | ((uint64_t) * INSTR->pRs << 32);
    RX_WriteACC(ACC);
    RX_REG_PC += 3;
}

void
rx_setup_mvtachi(void)
{
    unsigned int rs = ICODE24() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_mvtachi;
    INSTR->proc();
}

/** 
 **********************************************************************
 * \fn void rx_mvtaclo(void)
 * Move from source register to accumulator low
 * v0
 **********************************************************************
 */
static void
rx_mvtaclo(void)
{
    uint64_t ACC;
    ACC = RX_ReadACC() & ~UINT64_C(0xffffffff);
    ACC = ACC | *INSTR->pRs;
    RX_WriteACC(ACC);
    RX_REG_PC += 3;
}

void
rx_setup_mvtaclo(void)
{
    unsigned int rs = ICODE24() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_mvtaclo;
    INSTR->proc();
}

/**
 *************************************************************************
 * \fn void rx_mvtc_imm_dst(void)
 * (1) Move to control register. Sign extending immediate variant.
 * length 4 to 7 bytes.
 * v0
 *************************************************************************
 */
static void
rx_mvtc_imm32_dst(void)
{
    unsigned int cr = INSTR->arg1;
    int32_t value;
    value = RX_Read32(RX_REG_PC + 3);
    RX_REG_PC += 7;
    if (RX_REG_PSW & PSW_PM) {
        RX_WriteCRUM(value, cr);
    } else {
        RX_WriteCRSM(value, cr);
    }
}

static void
rx_mvtc_simm8_dst(void)
{
    unsigned int cr = INSTR->arg1;
    int32_t value;
    value = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 3);
    RX_REG_PC += 4;
    if (RX_REG_PSW & PSW_PM) {
        RX_WriteCRUM(value, cr);
    } else {
        RX_WriteCRSM(value, cr);
    }
}

static void
rx_mvtc_simm16_dst(void)
{
    unsigned int cr = INSTR->arg1;
    int32_t value;
    value = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 3);
    RX_REG_PC += 5;
    if (RX_REG_PSW & PSW_PM) {
        RX_WriteCRUM(value, cr);
    } else {
        RX_WriteCRSM(value, cr);
    }
}

static void
rx_mvtc_simm24_dst(void)
{
    unsigned int cr = INSTR->arg1;
    int32_t value;
    value = RX_Read24(RX_REG_PC + 3);
    value = (value << 8) >> 8;
    RX_REG_PC += 6;
    if (RX_REG_PSW & PSW_PM) {
        RX_WriteCRUM(value, cr);
    } else {
        RX_WriteCRSM(value, cr);
    }
}

void
rx_setup_mvtc_imm_dst(void)
{
    unsigned int li = (ICODE24() >> 10) & 3;
    unsigned int cr = ICODE24() & 0xf;
    switch (li) {
        case 0:
            INSTR->proc = rx_mvtc_imm32_dst;
            break;
        case 1:
            INSTR->proc = rx_mvtc_simm8_dst;
            break;
        case 2:
            INSTR->proc = rx_mvtc_simm16_dst;
            break;
        default:
        case 3:
            INSTR->proc = rx_mvtc_simm24_dst;
            break;
    }
    INSTR->arg1 = cr;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_mvtc_src_dst(void)
 * (2) Move to control register from a general purpose register
 * v0
 ********************************************************************
 */
static void
rx_mvtc_src_dst(void)
{
    unsigned int cr = INSTR->arg1;
    uint32_t value;
    value = *INSTR->pRs;
    if (RX_REG_PSW & PSW_PM) {
        RX_WriteCRUM(value, cr);
    } else {
        RX_WriteCRSM(value, cr);
    }
    RX_REG_PC += 3;
}

void
rx_setup_mvtc_src_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int cr = ICODE24() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = cr;
    INSTR->proc = rx_mvtc_src_dst;
    INSTR->proc();
}

/**
 **********************************************************************
 * void rx_mvtipl_src(void)
 * Move to interrupt privilege level from immediate encoded in 
 * instruction
 **********************************************************************
 */

static void
rx_mvtipl0_src(void)
{
    RX_SET_REG_PSW(RX_REG_PSW & ~PSW_IPL_MSK);
    RX_REG_PC += 3;
}

static void
rx_mvtipl1_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (1 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl2_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (2 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl3_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (3 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl4_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (4 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl5_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (5 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl6_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (6 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl7_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (7 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl8_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (8 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl9_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (9 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl10_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (10 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl11_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (11 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl12_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (12 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl13_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (13 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl14_src(void)
{
    RX_SET_REG_PSW((RX_REG_PSW & ~PSW_IPL_MSK) | (14 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

static void
rx_mvtipl15_src(void)
{
    RX_SET_REG_PSW(RX_REG_PSW | (15 << PSW_IPL_SHIFT));
    RX_REG_PC += 3;
}

void
rx_setup_mvtipl_src(void)
{
    unsigned int ipl = ICODE24() & 0xf;
    switch (ipl) {
        case 0:
            INSTR->proc = rx_mvtipl0_src;
            break;
        case 1:
            INSTR->proc = rx_mvtipl1_src;
            break;
        case 2:
            INSTR->proc = rx_mvtipl2_src;
            break;
        case 3:
            INSTR->proc = rx_mvtipl3_src;
            break;
        case 4:
            INSTR->proc = rx_mvtipl4_src;
            break;
        case 5:
            INSTR->proc = rx_mvtipl5_src;
            break;
        case 6:
            INSTR->proc = rx_mvtipl6_src;
            break;
        case 7:
            INSTR->proc = rx_mvtipl7_src;
            break;
        case 8:
            INSTR->proc = rx_mvtipl8_src;
            break;
        case 9:
            INSTR->proc = rx_mvtipl9_src;
            break;
        case 10:
            INSTR->proc = rx_mvtipl10_src;
            break;
        case 11:
            INSTR->proc = rx_mvtipl11_src;
            break;
        case 12:
            INSTR->proc = rx_mvtipl12_src;
            break;
        case 13:
            INSTR->proc = rx_mvtipl13_src;
            break;
        case 14:
            INSTR->proc = rx_mvtipl14_src;
            break;
        default:
        case 15:
            INSTR->proc = rx_mvtipl15_src;
            break;
    }
    INSTR->proc();
}

/**
 ************************************************************************
 * \fn void rx_neg_dst(void)
 * negate destination. Flags as in subtract.
 * v0
 ************************************************************************
 */
static void
rx_neg_dst(void)
{
    uint32_t Rd, result;
    Rd = *INSTR->pRd;
    result = -Rd;
    *INSTR->pRd = result;
    sub_flags(0, Rd, result);
    RX_REG_PC += 2;
}

void
rx_setup_neg_dst(void)
{
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_neg_dst;
    INSTR->proc();
}

/**
 ************************************************************
 * \fn void rx_neg_src_dst(void)
 * negate src and write it to a destination register.
 * v0
 ************************************************************
 */
static void
rx_neg_src_dst(void)
{
    uint32_t Rs, result;
    Rs = *INSTR->pRs;
    result = -Rs;
    *INSTR->pRd = result;
    sub_flags(0, Rs, result);
    RX_REG_PC += 3;
}

void
rx_setup_neg_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_neg_src_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_nop(void)
 * do nothing but increment the PC and consume time.
 * v0
 *************************************************************
 */
void
rx_nop(void)
{
    CycleCounter -= 1;          /* Hack */
    RX_REG_PC += 1;
}

/**
 *************************************************************
 * \fn void rx_not_dst(void)
 * Bitwise not of a register.
 * Length is 2 bytes. Flags are sign and zero.
 * v0
 *************************************************************
 */
static void
rx_not_dst(void)
{
    uint32_t Rd, result;
    Rd = *INSTR->pRd;
    result = ~Rd;
    *INSTR->pRd = result;
    not_flags(result);
    RX_REG_PC += 2;
}

void
rx_setup_not_dst(void)
{
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_not_dst;
    INSTR->proc();
}

/**
 ************************************************************************
 * \fn void rx_not_src_dst(void)
 * bitwise not of a register written into an other register
 * v0
 ************************************************************************
 */
static void
rx_not_src_dst(void)
{
    uint32_t Rs, result;
    Rs = *INSTR->pRs;
    result = ~Rs;
    *INSTR->pRd = result;
    not_flags(result);
    RX_REG_PC += 3;
}

void
rx_setup_not_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_not_src_dst;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_or_uimm4_dst(void)
 * (2) OR of a register with a 4 bit unsigned immediate.
 * v0
 ***********************************************************************
 */
static void
rx_or_uimm4_dst(void)
{
    uint32_t Rd, result;
    uint32_t uimm4 = INSTR->arg1;
    Rd = *INSTR->pRd;
    result = Rd | uimm4;
    or_flags(result);
    *INSTR->pRd = result;
    RX_REG_PC += 2;
}

void
rx_setup_or_uimm4_dst(void)
{
    unsigned int rd = (ICODE16() & 0xf);
    uint32_t uimm4 = (ICODE16() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = uimm4;
    INSTR->proc = rx_or_uimm4_dst;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_or_imm_dst(void)
 * (2) OR of a register with a sign extended immediate.
 * Length is 2 to 6 bytes
 * v0
 ***********************************************************************
 */
static void
rx_or_imm_dst(void)
{
    unsigned int li = INSTR->arg1;
    uint32_t Rd, result;
    uint32_t Src;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 2);
            RX_REG_PC += 6;
            break;

        case 1:
            Src = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:
            Src = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
        case 3:
            Src = RX_ReadSimm24(RX_REG_PC + 2);
            RX_REG_PC += 5;
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd | Src;
    *INSTR->pRd = result;
    or_flags(result);
}

void
rx_setup_or_imm_dst(void)
{
    unsigned int rd = (ICODE16() & 0xf);
    unsigned int li = (ICODE16() >> 8) & 0x3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = li;
    INSTR->proc = rx_or_imm_dst;
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_or_src_dst_ub(void)
 * (3a) OR of a register with an unsigned indirect addressed
 * byte with displacement or a Register.
 * v0
 ***************************************************************
 */
static void
rx_or_src_dst_ub(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 2;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 3;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 2;
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd | Src;
    or_flags(result);
    *INSTR->pRd = result;
}

void
rx_setup_or_src_dst_ub(void)
{
    unsigned int ld = (ICODE16() >> 8) & 0x3;
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int rd = (ICODE16() & 0xf);
    INSTR->arg1 = ld;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_or_src_dst_ub;
    INSTR->proc();
}

/**
 **************************************************************************
 * \fn void rx_or_src_dst_nub(void)
 * (3b) OR of an register with an indirectly addressed value with
 * displacement.
 * v0
 **************************************************************************
 */
static void
rx_or_src_dst_nub(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int mi = INSTR->arg2;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = *INSTR->pRs;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            fprintf(stderr, "OR: RS variant should not exist\n");
            RX_REG_PC += 3;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd | Src;
    *INSTR->pRd = result;
    or_flags(result);
}

void
rx_setup_or_src_dst_nub(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int mi = (ICODE24() >> 14) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    INSTR->arg1 = ld;
    INSTR->arg2 = mi;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_or_src_dst_nub;
    INSTR->proc();
}

/**
 **************************************************************************
 * \fn void rx_or_src_src2_dst(void)
 * (4) Or of two registers, result is written to a third register.
 * v0
 **************************************************************************
 */
static void
rx_or_src_src2_dst(void)
{
    uint32_t Src, Src2, result;
    Src = *INSTR->pRs;
    Src2 = *INSTR->pRs2;
    result = Src | Src2;
    *INSTR->pRd = result;
    or_flags(result);
    RX_REG_PC += 3;
}

void
rx_setup_or_src_src2_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = (ICODE24() & 0xf);
    unsigned int rd = (ICODE24() >> 8) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_or_src_src2_dst;
    INSTR->proc();
}

/**
 *****************************************************
 * \fn void rx_pop_dst(void)
 * Pop a register from stack.
 * v0
 *****************************************************
 */
static void
rx_pop_dst(void)
{
    uint32_t Sp;
    uint32_t value;
    Sp = RX_ReadReg(0);
    value = RX_Read32(Sp);
    RX_WriteReg(Sp + 4, 0);
    *INSTR->pRd = value;
    RX_REG_PC += 2;
}

void
rx_setup_pop_dst(void)
{
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_pop_dst;
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_popc_dst(void)
 * pop a control register from stack.
 * v0
 ***************************************************************
 */
static void
rx_popc_dst(void)
{
    unsigned int cr = INSTR->arg1;
    uint32_t sp;
    uint32_t value;
    sp = RX_ReadReg(0);
    value = RX_Read32(sp);
    RX_WriteReg(sp + 4, 0);
    if (RX_REG_PSW & PSW_PM) {
        RX_WriteCRUM(value, cr);
    } else {
        RX_WriteCRSM(value, cr);
    }
    RX_REG_PC += 2;
}

void
rx_setup_popc_dst(void)
{
    unsigned int cr = ICODE16() & 0xf;
    INSTR->arg1 = cr;
    INSTR->proc = rx_popc_dst;
    INSTR->proc();
}

/**
 ***************************************************************************
 * \fn void rx_popm_dst_dst2(void)
 * Pop a range of registers from stack.
 * from rd to rd2. where rd can not be 0 and rd2 can not be 0 or 1.
 * v0
 ***************************************************************************
 */
static void
rx_popm_dst_dst2(void)
{
    unsigned int rd = INSTR->arg1;
    unsigned int rd2 = INSTR->arg2;
    unsigned int i;
    uint32_t Sp, value;
    for (i = rd; i <= rd2; i++) {
        Sp = RX_ReadReg(0);
        value = RX_Read32(Sp);
        RX_WriteReg(Sp + 4, 0);
        RX_WriteReg(value, i);
    }
    RX_REG_PC += 2;
}

void
rx_setup_popm_dst_dst2(void)
{
    unsigned int rd2 = ICODE16() & 0xf;
    unsigned int rd = (ICODE16() >> 4) & 0xf;
    INSTR->arg1 = rd;
    INSTR->arg2 = rd2;
    INSTR->proc = rx_popm_dst_dst2;
    INSTR->proc();
}

/**
 *********************************************************************
 * \fn void rx_push_rs(void)
 * (1) push a register to the stack.
 * Always decrements SP by 4. If size is not .L the higher order
 * bits are undefined. Value should be checked with read hardware.
 * v0
 *********************************************************************
 */
static void
rx_push_b_rs(void)
{
    uint32_t Rs = *INSTR->pRs;
    uint32_t Sp = RX_ReadReg(0);
    Sp -= 4;
    RX_WriteReg(Sp, 0);
    RX_Write8(Rs, Sp);
    RX_REG_PC += 2;
}

static void
rx_push_w_rs(void)
{
    uint32_t Rs = *INSTR->pRs;
    uint32_t Sp = RX_ReadReg(0);
    Sp -= 4;
    RX_WriteReg(Sp, 0);
    RX_Write16(Rs, Sp);
    RX_REG_PC += 2;
}

static void
rx_push_l_rs(void)
{
    uint32_t Rs = *INSTR->pRs;
    uint32_t Sp = RX_ReadReg(0);
    Sp -= 4;
    RX_WriteReg(Sp, 0);
    RX_Write32(Rs, Sp);
    RX_REG_PC += 2;
}

void
rx_setup_push_rs(void)
{
    unsigned int rs = ICODE16() & 0xf;
    unsigned int sz = (ICODE16() >> 4) & 3;
    INSTR->pRs = RX_RegP(rs);
    switch (sz) {
        case 0:
            INSTR->proc = rx_push_b_rs;
            break;
        case 1:
            INSTR->proc = rx_push_w_rs;
            break;
        case 2:
            INSTR->proc = rx_push_l_rs;
            break;
        default:
            fprintf(stderr, "Error in %s\n", __func__);
            return;
    }
    INSTR->proc();
}

/**
 *********************************************************************
 * \fn void rx_push_isrc(void)
 * (2) Push an indirectly adressed source to the stack.
 * higher order bits in .W and .B case are undefined. Suspect 
 * sign extension, should check with real hw.
 * v0
 *********************************************************************
 */
static void
rx_push_isrc(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int sz = INSTR->arg2;
    uint32_t Sp;
    uint32_t dsp;
    uint32_t memaddr, Src;
    Sp = RX_ReadReg(0);
    Sp -= 4;
    RX_WriteReg(Sp, 0);
    memaddr = *INSTR->pRs;
    switch (ld) {
        case 0:                /* [Rs] */
            dsp = 0;
            RX_REG_PC += 2;
            break;
        case 1:                /* dsp:8[Rs] */
            dsp = RX_Read8(RX_REG_PC + 2);
            RX_REG_PC += 3;
            break;
        case 2:                /* dsp:16[Rs] */
            dsp = RX_Read16(RX_REG_PC + 2);
            RX_REG_PC += 4;
            break;
        default:
            dsp = 0;
            fprintf(stderr, "illegal ld in %s\n", __func__);
            break;
    }
    switch (sz) {
        case 0:                /* .B */
            Src = RX_Read8(memaddr + dsp);
            RX_Write8(Src, Sp);
            break;
        case 1:                /* .W */
            Src = RX_Read16(memaddr + (dsp << 1));
            RX_Write16(Src, Sp);
            break;
        case 2:                /* .L */
            Src = RX_Read32(memaddr + (dsp << 2));
            RX_Write32(Src, Sp);
            break;
        default:
            fprintf(stderr, "illegal sz in %s\n", __func__);
            break;

    }
}

void
rx_setup_push_isrc(void)
{
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int ld = (ICODE16() >> 8) & 3;
    unsigned int sz = (ICODE16() & 3);
    INSTR->pRs = RX_RegP(rs);
    INSTR->arg1 = ld;
    INSTR->arg2 = sz;
    INSTR->proc = rx_push_isrc;
    INSTR->proc();
}

/**
 ****************************************************************
 * \fn void rx_pushc_src(void)
 * Push a control register to the stack.
 * v0
 ****************************************************************
 */
static void
rx_pushc_src(void)
{
    unsigned int cr = INSTR->arg1;
    uint32_t Sp;
    uint32_t value;
    Sp = RX_ReadReg(0);
    Sp -= 4;
    RX_WriteReg(Sp, 0);
    value = RX_ReadCR(cr);
    RX_Write32(value, Sp);
    RX_REG_PC += 2;
}

void
rx_setup_pushc_src(void)
{
    unsigned int cr = ICODE16() & 0xf;
    INSTR->arg1 = cr;
    INSTR->proc = rx_pushc_src;
    INSTR->proc();
}

/**
 **************************************************************
 * \fn void rx_pushm_src_src2(void)
 * Push multiple registers to the stack.
 * rs from 1 to 14 rs2 from 2 to 15
 * v0
 **************************************************************
 */
static void
rx_pushm_src_src2(void)
{
    unsigned int rs = INSTR->arg1;
    unsigned int rs2 = INSTR->arg2;
    unsigned int i;
    uint32_t Sp = RX_ReadReg(0);
    for (i = rs2; i >= rs; i--) {
        Sp = Sp - 4;
        RX_Write32(RX_ReadReg(i), Sp);
    }
    RX_WriteReg(Sp, 0);
    RX_REG_PC += 2;
}

void
rx_setup_pushm_src_src2(void)
{
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int rs2 = ICODE16() & 0xf;
    INSTR->proc = rx_pushm_src_src2;
    INSTR->arg1 = rs;
    INSTR->arg2 = rs2;
    INSTR->proc();
}

/**
 *****************************************************************
 * Round the accumulator word
 *****************************************************************
 */
static void
rx_racw_src(void)
{
    uint32_t imm1 = INSTR->arg1;
    uint32_t shift = imm1 + 1;
    int64_t tmp, ACC;
    ACC = RX_ReadACC();
    tmp = ACC << shift;
    tmp += INT64_C(0x80000000);
    if (tmp > INT64_C(0x00007FFFF00000000)) {
        ACC = INT64_C(0x00007FFFF00000000);
    } else if (tmp < INT64_C(0xFFFF800000000000)) {
        ACC = INT64_C(0xFFFF800000000000);
    } else {
        ACC = tmp & INT64_C(0xFFFFFFFF00000000);
    }
    RX_WriteACC(ACC);
    RX_REG_PC += 3;
}

void
rx_setup_racw_src(void)
{
    uint32_t imm1 = (ICODE24() >> 4) & 1;
    INSTR->arg1 = imm1;
    INSTR->proc = rx_racw_src;
    INSTR->proc();
}

/**
 ******************************************************************
 * \fn void rx_revl_src_dst(void)
 * Byte order conversion long variant. No influence on flags.
 * v0
 ******************************************************************
 */
static void
rx_revl_src_dst(void)
{
    uint32_t Rs;
    Rs = *INSTR->pRs;
    *INSTR->pRd = swap32(Rs);
    RX_REG_PC += 3;
}

void
rx_setup_revl_src_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_revl_src_dst;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_revw_src_dst(void)
 * Byte order conversion. Swap bytes of upper and lower word pairwise 
 * v0
 **********************************************************************
 */
static void
rx_revw_src_dst(void)
{
    uint32_t Rs, Rd;
    Rs = *INSTR->pRs;
    Rd = ((Rs & 0xff00ff00) >> 8) | ((Rs & 0x00ff00ff) << 8);
    *INSTR->pRd = Rd;
    RX_REG_PC += 3;
}

void
rx_setup_revw_src_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_revw_src_dst;
    INSTR->proc();
}

static void
rx_rmpa(void)
{
    unsigned int sz = INSTR->arg1;
    unsigned int n;
    uint32_t R1, R2, R3, R4, R5, R6;
    int64_t add;
    uint64_t R5R4;
    uint64_t result;
    R3 = RX_ReadReg(3);
    if (R3 == 0) {
        RX_REG_PC += 2;
        return;
    }
    R1 = RX_ReadReg(1);
    R2 = RX_ReadReg(2);
    R4 = RX_ReadReg(4);
    R5 = RX_ReadReg(5);
    R6 = RX_ReadReg(6);
    R5R4 = R4 | ((uint64_t) R5 << 32);
    switch (sz) {
        case 0:                /* Byte */
            add = (int64_t) (int8_t) RX_Read8(R1) * (int64_t) (int8_t) RX_Read8(R2);
            n = 1;
            break;
        case 1:                /* Word */
            add = (int64_t) (int16_t) RX_Read16(R1) * (int64_t) (int16_t) RX_Read16(R2);
            n = 2;
            break;
        case 2:                /* Long */
            add = (int64_t) (int32_t) RX_Read32(R1) * (int64_t) (int32_t) RX_Read32(R2);
            n = 4;
            break;
        default:
            fprintf(stderr, "Illegal sz in rmpa\n");
            add = 0;
            n = 0;
            break;
    }
    result = R5R4 + (uint64_t) add;
    /* Carry */
    if (R6 & 0x80000000) {
        R6--;
    }
    if (((ISNEG64(R5R4) && ISNEG64(add))
         || (ISNEG64(R5R4) && ISNOTNEG64(result))
         || (ISNEG64(add) && ISNOTNEG64(result)))) {
        R6 = R6 + 1;
    }
    R6 = (int32_t) (int16_t) R6;
    if (ISNEG(R6)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    if ((R6 == 0) && ((int64_t) result >= 0)) {
        RX_REG_PSW &= ~PSW_O;
    } else if ((R6 == ~UINT32_C(0)) && ((int64_t) result < 0)) {
        RX_REG_PSW &= ~PSW_O;
    } else {
        RX_REG_PSW |= PSW_O;
    }
    R1 = R1 + n;
    R2 = R2 + n;
    R3 = R3 - 1;
    R5 = result >> 32;
    R4 = (uint32_t) result;
    RX_WriteReg(R1, 1);
    RX_WriteReg(R2, 2);
    RX_WriteReg(R3, 3);
    RX_WriteReg(R4, 4);
    RX_WriteReg(R5, 5);
    RX_WriteReg(R6, 6);
    RX_REG_PC += 2;
}

void
rx_setup_rmpa(void)
{
    unsigned int sz = ICODE16() & 0x3;
    INSTR->arg1 = sz;
    INSTR->proc = rx_rmpa;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_rolc_dst(void)
 * Rotate left with carry in the chain.
 * v0
 ***********************************************************************
 */
static void
rx_rolc_dst(void)
{
    unsigned int carry_new;
    uint32_t Rd = *INSTR->pRd;
    carry_new = ! !(Rd & 0x80000000);
    if (RX_REG_PSW & PSW_C) {
        Rd = (Rd << 1) | 1;
    } else {
        Rd = (Rd << 1);
    }
    *INSTR->pRd = Rd;
    if (carry_new) {
        RX_REG_PSW |= PSW_C;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 2;
}

void
rx_setup_rolc_dst(void)
{
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_rolc_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * Rotate right with carry in the chain.
 * v0
 *************************************************************
 */
static void
rx_rorc_dst(void)
{
    unsigned int rd = ICODE16() & 0xf;
    unsigned int carry_new;
    uint32_t Rd = RX_ReadReg(rd);
    carry_new = Rd & 1;
    if (RX_REG_PSW & PSW_C) {
        Rd = (Rd >> 1) | 0x80000000;
    } else {
        Rd = (Rd >> 1);
    }
    RX_WriteReg(Rd, rd);
    if (carry_new) {
        RX_REG_PSW |= PSW_C;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 2;
}

void
rx_setup_rorc_dst(void)
{
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_rorc_dst;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_rotl_imm5_dst(void)
 * Rotate left by an 5 bit immediate. The carry is modified but not
 * in the rotation. It will have the same value as the LSB even if
 * rotation is 0. 
 * v0
 ***********************************************************************
 */
static void
rx_rotl_imm5_dst(void)
{
    unsigned int rot = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    if (rot) {
        Rd = (Rd << rot) | (Rd >> (32 - rot));
        *INSTR->pRd = Rd;
    }
    if (Rd & 1) {
        RX_REG_PSW |= PSW_C;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 3;
}

void
rx_setup_rotl_imm5_dst(void)
{
    unsigned int rot = (ICODE24() >> 4) & 31;
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = rot;
    INSTR->proc = rx_rotl_imm5_dst;
    INSTR->proc();
}

/**
 *****************************************************************
 * Rotate left by a register value. Only the lower 5 bits of
 * src are used.
 * v0
 *****************************************************************
 */
static void
rx_rotl_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rot;
    uint32_t Rd = RX_ReadReg(rd);
    rot = RX_ReadReg(rs) & 31;
    if (rot) {
        Rd = (Rd << rot) | (Rd >> (32 - rot));
        RX_WriteReg(Rd, rd);
    }
    if (Rd & 1) {
        RX_REG_PSW |= PSW_C;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 3;
}

void
rx_setup_rotl_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_rotl_src_dst;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_rotr_imm5_dst(void)
 * Rotate right by a five bit immediate. The carry will have
 * the same value as the MSB of dest even if rot is 0.
 * v0
 *****************************************************************
 */
static void
rx_rotr_imm5_dst(void)
{
    unsigned int rot = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    if (rot) {
        Rd = (Rd >> rot) | (Rd << (32 - rot));
        *INSTR->pRd = Rd;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
        RX_REG_PSW |= PSW_C;
    } else {
        RX_REG_PSW &= ~PSW_S;
        RX_REG_PSW &= ~PSW_C;
    }
    RX_REG_PC += 3;
}

void
rx_setup_rotr_imm5_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rot = (ICODE24() >> 4) & 31;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = rot;
    INSTR->proc = rx_rotr_imm5_dst;
    INSTR->proc();
}

/**
 *************************************************************************
 * \fn void rx_rortr_src_dst(void)
 * Rotate right by the lower five bits from a register value.
 * The carry will have the same value as the MSB even if rot is 0.
 * v0
 *************************************************************************
 */
static void
rx_rortr_src_dst(void)
{
    unsigned int rot;
    uint32_t Rd = *INSTR->pRd;
    rot = *INSTR->pRs & 31;
    if (rot) {
        Rd = (Rd >> rot) | (Rd << (32 - rot));
        *INSTR->pRd = Rd;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
        RX_REG_PSW |= PSW_C;
    } else {
        RX_REG_PSW &= ~PSW_S;
        RX_REG_PSW &= ~PSW_C;
    }
    RX_REG_PC += 3;
}

void
rx_setup_rortr_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_rortr_src_dst;
    INSTR->proc();
}

/**
 *******************************************************************
 * Convert a floating point number to a signed integer mode
 * according to rounding mode from FPSW.
 * v0
 *******************************************************************
 */
static void
rx_round_src_dst(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t dsp;
    uint32_t Rs = *INSTR->pRs;
    Float32_t fSrc;
    uint32_t Rd;
    switch (ld) {
        case 0:
            fSrc = RX_CastToFloat32(RX_Read32(Rs));
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            fSrc = RX_CastToFloat32(RX_Read32(Rs + (dsp << 2)));
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            fSrc = RX_CastToFloat32(Rs);
            RX_REG_PC += 3;
            break;
    }
    Rd = Float32_ToInt32(RX_FLOAT_CONTEXT, fSrc, SoftFloat_RoundingMode(RX_FLOAT_CONTEXT));
    *INSTR->pRd = Rd;
}

void
rx_setup_round_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_round_src_dst;
    INSTR->proc();
}

/**
 ****************************************************************
 * \fn void rx_rte(void)
 * Return from exception. 
 * v0
 ****************************************************************
 */
void
rx_rte(void)
{
    uint32_t Sp;
    uint32_t tmp;
    if (unlikely(RX_REG_PSW & PSW_PM)) {
        RX_PrivilegedInstructionException();
        return;
    }
    Sp = RX_ReadReg(0);
    RX_REG_PC = RX_Read32(Sp);
    Sp += 4;
    RX_WriteReg(Sp, 0);
    tmp = RX_Read32(Sp);
    Sp += 4;
    RX_WriteReg(Sp, 0);
    /* 
     **************************************************************
     * On transition to User mode switch the stack pointer to
     * user stack. This is verified with real device.
     **************************************************************
     */
    if (tmp & PSW_PM) {
        tmp = tmp | PSW_U;
    }
    RX_SET_REG_PSW(tmp);
}

/**
 **************************************************************
 * \fn void rx_rtfi(void)
 * Return from fast interrupt
 **************************************************************
 */
void
rx_rtfi(void)
{
    if (unlikely(RX_REG_PSW & PSW_PM)) {
        RX_PrivilegedInstructionException();
        return;
    }
    if (RX_REG_BPSW & PSW_PM) {
        RX_SET_REG_PSW(RX_REG_BPSW | PSW_U);
    } else {
        RX_SET_REG_PSW(RX_REG_BPSW);
    }
    RX_REG_PC = RX_REG_BPC;
}

/**
 ************************************************************************
 * \fn void rx_rts(void)
 * Return from subroutine.
 * v0
 ************************************************************************
 */
void
rx_rts(void)
{
    uint32_t Sp = RX_ReadReg(0);
    RX_WriteReg(Sp + 4, 0);
    RX_REG_PC = RX_Read32(Sp);
}

/*
 ***********************************************************************
 * \fn void rx_rtsd_src(void)
 * return from subroutine and deallocate stackframe.
 * v0
 ***********************************************************************
 */
void
rx_rtsd_src(void)
{
    uint32_t uimm = RX_Read8(RX_REG_PC + 1) << 2;
    uint32_t Sp = RX_ReadReg(0);
    Sp += uimm;
    RX_REG_PC = RX_Read32(Sp);
    Sp += 4;
    RX_WriteReg(Sp, 0);
}

/**
 *************************************************************
 * \fn void rx_rtsd_src_dst_dst2(void)
 * Return from subroutine deallocating the stack frame
 * v0
 *************************************************************
 */
void
rx_rtsd_src_dst_dst2(void)
{
    unsigned int i;
    unsigned int rd = INSTR->arg1;
    unsigned int rd2 = INSTR->arg2;
    uint32_t uimm = RX_Read8(RX_REG_PC + 2) << 2;
    uint32_t Sp = RX_ReadReg(0);
    uint32_t tmp;
    Sp += uimm;
    Sp -= (rd2 - rd + 1) << 2;
    for (i = rd; i <= rd2; i++) {
        tmp = RX_Read32(Sp);
        Sp += 4;
        RX_WriteReg(tmp, i);
    }
    RX_REG_PC = RX_Read32(Sp);
    Sp += 4;
    RX_WriteReg(Sp, 0);
}

void
rx_setup_rtsd_src_dst_dst2(void)
{
    unsigned int rd = (ICODE16() >> 4) & 0xf;
    unsigned int rd2 = ICODE16() & 0xf;
    INSTR->arg1 = rd;
    INSTR->arg2 = rd2;
    INSTR->proc = rx_rtsd_src_dst_dst2;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_sat_dst(void)
 * saturation operation. If overflow flag is set then
 * the result is INT_MAX or INT_MIN depending on sign flag.
 * v0
 *************************************************************
 */
void
rx_sat_dst(void)
{
    unsigned int rd;
    if (RX_REG_PSW & PSW_O) {
        rd = ICODE16() & 0xf;
        if (RX_REG_PSW & PSW_S) {
            RX_WriteReg(0x7fffffff, rd);
        } else {
            RX_WriteReg(0x80000000, rd);
        }
    }
}

/**
 ********************************************************************
 * Saturation of 64 bit R6:R5:R4 
 * If overflow flag is set write the saturation values
 * to the registers.
 ********************************************************************
 */
void
rx_satr(void)
{
    if (RX_REG_PSW & PSW_O) {
        if (RX_REG_PSW & PSW_S) {
            RX_WriteReg(0xFFFFFFFF, 6);
            RX_WriteReg(0x80000000, 5);
            RX_WriteReg(0x00000000, 4);
        } else {
            RX_WriteReg(0x00000000, 6);
            RX_WriteReg(0x7FFFFFFF, 5);
            RX_WriteReg(0xFFFFFFFF, 4);
        }
    }
}

/**
 *********************************************************************
 * \fn void rx_sbb_src_dst(void)
 * (1) Subtract with borrow. Src and destination are registers.
 *********************************************************************
 */
static void
rx_sbb_src_dst(void)
{
    uint32_t Rs, Rd;
    uint32_t result;
    Rs = *INSTR->pRs;
    Rd = *INSTR->pRd;
    result = Rd - Rs;
    if (!(RX_REG_PSW & PSW_C)) {
        result = result - 1;
    }
    *INSTR->pRd = result;
    sub_flags(Rd, Rs, result);
    RX_REG_PC += 3;
}

void
rx_setup_sbb_src_dst(void)
{
    unsigned int rd, rs;
    rs = (ICODE24() >> 4) & 0xf;
    rd = (ICODE24() & 0xf);
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_sbb_src_dst;
    INSTR->proc();
}

/**
 **********************************************************************
 * \fn void rx_sbb_isrc_dst(void)
 * (2) Subtract with borrow with indirect addressed long source with
 * displacement. length 4 to 6 bytes
 * v0
 **********************************************************************
 */
void
rx_sbb_isrc_dst(void)
{
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    unsigned int ld = (ICODE32() >> 16) & 3;
    uint32_t Src, Rd;
    uint32_t result;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    Rd = RX_ReadReg(rd);
    switch (ld) {
        case 0:
            RX_REG_PC += 4;
            break;
        case 1:
            memaddr += RX_Read8(RX_REG_PC + 4) << 2;
            RX_REG_PC += 5;
            break;
        case 2:
            memaddr += RX_Read16(RX_REG_PC + 4) << 2;
            RX_REG_PC += 6;
            break;
        default:
            /* Make silly compiler happy */
            break;
    }
    Src = RX_Read32(memaddr);
    result = Rd - Src;
    if (!(RX_REG_PSW & PSW_C)) {
        result--;
    }
    sub_flags(Rd, Src, result);
    RX_WriteReg(result, rd);
}

/**
 **********************************************************************
 * \fn void rx_sccnd_dst(void)
 * Store 0 if condition is false else 1. 
 **********************************************************************
 */
static void
rx_sccnd_dst(void)
{
    unsigned int cnd = INSTR->arg1;
    unsigned int ld = INSTR->arg2;
    unsigned int sz = INSTR->arg3;
    uint32_t value;
    uint32_t dsp;
    uint32_t addr;
    uint32_t Rd;
    if (check_condition(cnd)) {
        value = 1;
    } else {
        value = 0;
    }
    switch (ld) {
        case 0:
            RX_REG_PC += 3;
            dsp = 0;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
            /* ugly */
        default:
        case 3:
            *INSTR->pRd = value;
            RX_REG_PC += 3;
            return;
    }
    Rd = *INSTR->pRd;
    switch (sz) {
        case 0:
            addr = Rd + dsp;
            RX_Write8(value, addr);
            break;
        case 1:
            addr = Rd + (dsp << 1);
            RX_Write16(value, addr);
            break;
        case 2:
            addr = Rd + (dsp << 2);
            RX_Write32(value, addr);
            break;
        default:
            fprintf(stderr, "Illegal size in instruction\n");
            return;
    }
}

void
rx_setup_sccnd_dst(void)
{
    unsigned int rd = (ICODE24() >> 4) & 0xf;
    unsigned int cnd = ICODE24() & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    unsigned int sz = (ICODE24() >> 10) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = cnd;
    INSTR->arg2 = ld;
    INSTR->arg3 = sz;
    INSTR->proc = rx_sccnd_dst;
    INSTR->proc();
}

/** 
 **********************************************************************
 * String compare until not equal.
 * v0
 **********************************************************************
 */
void
rx_scmpu(void)
{
    uint32_t R1, R2, R3;
    R1 = RX_ReadReg(1);
    R2 = RX_ReadReg(2);
    R3 = RX_ReadReg(3);
    uint8_t tmp0, tmp1;
    if (R3 != 0) {
        tmp0 = RX_Read8(R1);
        tmp1 = RX_Read8(R2);
        RX_WriteReg(R1 + 1, 1);
        RX_WriteReg(R2 + 1, 2);
        RX_WriteReg(R3 - 1, 3);
        if ((tmp0 != tmp1) || (tmp0 == 0)) {
            RX_REG_PC += 2;
        }
        /* 
         ******************************************************
         * Don't know what happens with the flags
         * if an interrupt occurs during the operation 
         * maybe this is only updated after last compare.
         ******************************************************
         */
        if ((tmp0 - tmp1) >= 0) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        if (tmp0 == tmp1) {
            RX_REG_PSW |= PSW_Z;
        } else {
            RX_REG_PSW &= ~PSW_Z;
        }
    } else {
        RX_REG_PC += 2;
    }
}

/**
 **********************************************************************
 * \fn void rx_setpsw(void)
 * Set a bit in the processor status word.
 **********************************************************************
 */
static void
rx_setpsw(void)
{
    unsigned int cb = INSTR->arg1;
    if (cb < 4) {
        RX_REG_PSW |= (1 << cb);
    } else if (cb == 8) {
        if (!(RX_REG_PSW & PSW_PM)) {
            RX_SET_REG_PSW(RX_REG_PSW | PSW_I);
        } else {
            fprintf(stderr, "Warning, setting PSW_I in Usermode\n");
        }
    } else if (cb == 9) {
        if (!(RX_REG_PSW & PSW_PM)) {
            RX_SET_REG_PSW(RX_REG_PSW | PSW_U);
        } else {
            fprintf(stderr, "Warning, setting PSW_U in Usermode\n");
        }
    }
    RX_REG_PC += 2;
}

void
rx_setup_setpsw(void)
{
    unsigned int cb = ICODE16() & 0xf;
    INSTR->arg1 = cb;
    INSTR->proc = rx_setpsw;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_shar_imm5_dst(void)
 * (1) Shift arithmetic right by a five bit immediate.
 * The manual is not 100 percent clear if this is a jamming shift
 * into the carry.
 * v0
 ***********************************************************************
 */
static void
rx_shar_imm5_dst(void)
{
    unsigned int shift = INSTR->arg1;
    int32_t Rd = *INSTR->pRd;
    if (shift) {
        if ((Rd >> (shift - 1)) & 1) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Rd = ((int32_t) Rd >> shift);
        *INSTR->pRd = Rd;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 2;
}

void
rx_setup_shar_imm5_dst(void)
{
    unsigned int shift = (ICODE16() >> 4) & 31;
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = shift;
    INSTR->proc = rx_shar_imm5_dst;
    INSTR->proc();
}

/**
 ****************************************************************
 * \fn void rx_shar_src_dst(void)
 * Shift arithmetic right of a register.
 * v0
 ****************************************************************
 */
static void
rx_shar_src_dst(void)
{
    unsigned int shift;
    int32_t Rd = *INSTR->pRd;
    shift = *INSTR->pRs & 31;
    if (shift) {
        if ((Rd >> (shift - 1)) & 1) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Rd = ((int32_t) Rd >> shift);
        *INSTR->pRd = Rd;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 3;
}

void
rx_setup_shar_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_shar_src_dst;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_shar_imm5_src2_dst(void)
 * Shift arithmetic right of a register. Result is written to
 * an other register.
 * v0
 *******************************************************************
 */
static void
rx_shar_imm5_src2_dst(void)
{
    unsigned int shift = INSTR->arg1;
    int32_t Rd;
    Rd = *INSTR->pRs2;
    if (shift) {
        if ((Rd >> (shift - 1)) & 1) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Rd = ((int32_t) Rd >> shift);
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    *INSTR->pRd = Rd;
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 3;
}

void
rx_setup_shar_imm5_src2_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs2 = (ICODE24() >> 4) & 0xf;
    unsigned int shift = (ICODE24() >> 8) & 31;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->arg1 = shift;
    INSTR->proc = rx_shar_imm5_src2_dst;
    INSTR->proc();
}

/**
 ************************************************************
 * \fn void rx_shll_imm5_dst(void)
 * Shift a register left by a five bit immediate.
 * v0
 ************************************************************
 */
static void
rx_shll_imm5_dst(void)
{
    unsigned int shift = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    if (shift) {
        if ((Rd << (shift - 1)) & 0x80000000) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Rd = (Rd << shift);
        *INSTR->pRd = Rd;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 2;
}

void
rx_setup_shll_imm5_dst(void)
{
    unsigned int shift = (ICODE16() >> 4) & 31;
    unsigned int rd = ICODE16() & 0xf;
    INSTR->arg1 = shift;
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_shll_imm5_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_shll_src_dst(void)
 * (2) Shift left Rd by Rs
 * v0
 *************************************************************
 */
static void
rx_shll_src_dst(void)
{
    unsigned int shift;
    uint32_t Rd = *INSTR->pRd;
    shift = *INSTR->pRs & 31;
    if (shift) {
        if ((Rd << (shift - 1)) & 0x80000000) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Rd = (Rd << shift);
        *INSTR->pRd = Rd;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 3;
}

void
rx_setup_shll_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_shll_src_dst;
    INSTR->proc();
}

/**
 ***************************************************************
 * \fn void rx_shll_imm5_src2_dst(void)
 * (3) Shift left of Src2 by a five bit immediate, result to Rd.
 * v0
 ***************************************************************
 */
static void
rx_shll_imm5_src2_dst(void)
{
    unsigned int shift = INSTR->arg1;
    uint32_t Src2;
    Src2 = *INSTR->pRs2;
    if (shift) {
        if ((Src2 << (shift - 1)) & 0x80000000) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Src2 = (Src2 << shift);
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    *INSTR->pRd = Src2;
    if (Src2 == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Src2)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 3;
}

void
rx_setup_shll_imm5_src2_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs2 = (ICODE24() >> 4) & 0xf;
    unsigned int shift = (ICODE24() >> 8) & 31;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->arg1 = shift;
    INSTR->proc = rx_shll_imm5_src2_dst;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_shlr_imm5_dst(void)
 * Shift logical right by a five bit immediate
 * (v0) 
 ********************************************************************
 */
static void
rx_shlr_imm5_dst(void)
{
    unsigned int shift = INSTR->arg1;
    uint32_t Rd = *INSTR->pRd;
    if (shift) {
        if ((Rd >> (shift - 1)) & 1) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Rd = Rd >> shift;
        *INSTR->pRd = Rd;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 2;
}

void
rx_setup_shlr_imm5_dst(void)
{
    unsigned int shift = (ICODE16() >> 4) & 31;
    unsigned int rd = ICODE16() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = shift;
    INSTR->proc = rx_shlr_imm5_dst;
    INSTR->proc();
}

/**
 ******************************************************************
 * \fn void rx_shlr_src_dst(void)
 * (2) Shift logical right Rd by Rs
 * v0
 ******************************************************************
 */
static void
rx_shlr_src_dst(void)
{
    unsigned int shift;
    uint32_t Rd = *INSTR->pRd;
    shift = *INSTR->pRs & 31;
    if (shift) {
        if ((Rd >> (shift - 1)) & 1) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Rd = Rd >> shift;
        *INSTR->pRd = Rd;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    if (Rd == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Rd)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 3;
}

void
rx_setup_shlr_src_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs = RX_RegP(rs);
    INSTR->proc = rx_shlr_src_dst;
    INSTR->proc();
}

/**
 *************************************************************
 * \fn void rx_shlr_imm5_src2_dst(void)
 * (3) shift logical right Src2 by Src and write to Rd 
 *************************************************************
 */
static void
rx_shlr_imm5_src2_dst(void)
{
    unsigned int shift = INSTR->arg1;
    uint32_t Src2;
    Src2 = *INSTR->pRs2;
    if (shift) {
        if ((Src2 >> (shift - 1)) & 1) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        Src2 = Src2 >> shift;
    } else {
        RX_REG_PSW &= ~PSW_C;
    }
    *INSTR->pRd = Src2;
    if (Src2 == 0) {
        RX_REG_PSW |= PSW_Z;
    } else {
        RX_REG_PSW &= ~PSW_Z;
    }
    if (ISNEG(Src2)) {
        RX_REG_PSW |= PSW_S;
    } else {
        RX_REG_PSW &= ~PSW_S;
    }
    RX_REG_PC += 3;
}

void
rx_setup_shlr_imm5_src2_dst(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs2 = (ICODE24() >> 4) & 0xf;
    unsigned int shift = (ICODE24() >> 8) & 31;
    INSTR->pRd = RX_RegP(rd);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->arg1 = shift;
    INSTR->proc = rx_shlr_imm5_src2_dst;
    INSTR->proc();
}

/**
 ************************************************************
 * \fnvoid rx_smovb(void)
 * String move backward.
 * v0
 ************************************************************
 */
void
rx_smovb(void)
{
    uint32_t R1, R2, R3;
    R1 = RX_ReadReg(1);
    R2 = RX_ReadReg(2);
    R3 = RX_ReadReg(3);
    if (R3) {
        RX_Write8(RX_Read8(R2), R1);
        RX_WriteReg(R1 - 1, 1);
        RX_WriteReg(R2 - 1, 2);
        RX_WriteReg(R3 - 1, 3);
    } else {
        RX_REG_PC += 2;
    }
}

/**
 *******************************************************
 * \fn void rx_smovf(void)
 * String move forward.
 * v0
 *******************************************************
 */
void
rx_smovf(void)
{
    uint32_t R1, R2, R3;
    R1 = RX_ReadReg(1);
    R2 = RX_ReadReg(2);
    R3 = RX_ReadReg(3);
    if (R3) {
        RX_Write8(RX_Read8(R2), R1);
        RX_WriteReg(R1 + 1, 1);
        RX_WriteReg(R2 + 1, 2);
        RX_WriteReg(R3 - 1, 3);
    } else {
        RX_REG_PC += 2;
    }
}

/**
 ***********************************************************
 * \fn void rx_smovu(void)
 * String move while unequal zero.
 * v0
 ***********************************************************
 */
void
rx_smovu(void)
{
    uint32_t R1, R2, R3;
    uint8_t tmp;
    R1 = RX_ReadReg(1);
    R2 = RX_ReadReg(2);
    R3 = RX_ReadReg(3);
    if (R3) {
        tmp = RX_Read8(R2);
        RX_WriteReg(R2 + 1, 2);
        RX_Write8(tmp, R1);
        RX_WriteReg(R1 + 1, 1);
        RX_WriteReg(R3 - 1, 3);
        if (tmp == 0) {
            RX_REG_PC += 2;
        }
    } else {
        RX_REG_PC += 2;
    }
}

/**
 **************************************************************************
 * \fn void rx_sstr(void)
 * Store a string
 * v0
 **************************************************************************
 */
static void
rx_sstr(void)
{
    uint32_t R1, R2, R3;
    unsigned int sz = INSTR->arg1;
    R1 = RX_ReadReg(1);
    R2 = RX_ReadReg(2);
    R3 = RX_ReadReg(3);
    if (R3) {
        switch (sz) {
            case 0:
                RX_Write8(R2, R1);
                R1++;
                break;

            case 1:
                RX_Write16(R2, R1);
                R1 += 2;
                break;

            case 2:
                RX_Write32(R2, R1);
                R1 += 4;
                break;

            default:
                fprintf(stderr, "Illegal sz in sstr\n");
                break;
        }
        RX_WriteReg(R1, 1);
        RX_WriteReg(R3 - 1, 3);
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_setup_sstr(void)
{
    unsigned int sz = ICODE16() & 3;
    INSTR->arg1 = sz;
    INSTR->proc = rx_sstr;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_stnz_src_dst(void)
 * store when not zero.
 ********************************************************************
 */
static void
rx_stnz_src_dst(void)
{
    unsigned int li = INSTR->arg1;
    int32_t simm;
    uint32_t dpc;
    switch (li) {
        case 0:                /* IMM32 */
            dpc = 7;
            break;
        case 1:                /* SIMM8 */
            dpc = 4;
            break;
        case 2:                /* SIMM16 */
            dpc = 5;
            break;
        default:
        case 3:                /* SIMM24 */
            dpc = 6;
            break;
    }
    if (!(RX_REG_PSW & PSW_Z)) {
        switch (li) {
            case 0:            /* IMM32 */
                simm = RX_Read32(RX_REG_PC + 3);
                break;
            case 1:            /* SIMM8 */
                simm = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 3);
                break;
            case 2:            /* SIMM16 */
                simm = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 3);
                break;
            default:
            case 3:            /* SIMM24 */
                simm = ((int32_t) (RX_Read24(RX_REG_PC + 3) << 8)) >> 8;
                break;
        }
        *INSTR->pRd = simm;
    }
    RX_REG_PC += dpc;
}

void
rx_setup_stnz_src_dst(void)
{
    unsigned int li = (ICODE24() >> 10) & 3;
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = li;
    INSTR->proc = rx_stnz_src_dst;
    INSTR->proc();
}

/**
 *****************************************************************
 * \fn void rx_stz_src_dst(void)
 * Store when zero
 *****************************************************************
 */
static void
rx_stz_src_dst(void)
{
    unsigned int li = INSTR->arg1;
    int32_t simm;
    uint32_t dpc;
    switch (li) {
        case 0:                /* IMM32 */
            dpc = 7;
            break;
        case 1:                /* SIMM8 */
            dpc = 4;
            break;
        case 2:                /* SIMM16 */
            dpc = 5;
            break;
        default:
        case 3:                /* SIMM24 */
            dpc = 6;
            break;
    }
    if (RX_REG_PSW & PSW_Z) {
        switch (li) {
            case 0:            /* IMM32 */
                simm = RX_Read32(RX_REG_PC + 3);
                break;
            case 1:            /* SIMM8 */
                simm = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 3);
                break;
            case 2:            /* SIMM16 */
                simm = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 3);
                break;
            default:
            case 3:            /* SIMM24 */
                simm = ((int32_t) (RX_Read24(RX_REG_PC + 3) << 8)) >> 8;
                break;
        }
        *INSTR->pRd = simm;
    }
    RX_REG_PC += dpc;
}

void
rx_setup_stz_src_dst(void)
{
    unsigned int li = (ICODE24() >> 10) & 3;
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = li;
    INSTR->proc = rx_stz_src_dst;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_sub_uimm4_dst(void)
 * (1) subtract an uimm4 from a destination register
 * v0
 ***********************************************************************
 */
static void
rx_sub_uimm4_dst(void)
{
    uint32_t uimm = INSTR->arg1;
    uint32_t Rd, result;
    Rd = *INSTR->pRd;
    result = Rd - uimm;
    *INSTR->pRd = result;
    sub_flags(Rd, uimm, result);
    RX_REG_PC += 2;
}

void
rx_setup_sub_uimm4_dst(void)
{
    unsigned int rd = (ICODE16() & 0xf);
    uint32_t uimm = (ICODE16() >> 4) & 0xf;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = uimm;
    INSTR->proc = rx_sub_uimm4_dst;
    INSTR->proc();
}

/**
 *************************************************************************
 * \fn void rx_sub_src_dst_ub(void)
 * Subtract an indirectly addressed unsigned byte or Rs from Rd.
 * v0
 *************************************************************************
 */
static void
rx_sub_src_dst_ub(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 2;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 3;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 2);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 2;
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd - Src;
    *INSTR->pRd = result;
    sub_flags(Rd, Src, result);
}

void
rx_setup_sub_src_dst_ub(void)
{
    unsigned int rs = (ICODE16() >> 4) & 0xf;
    unsigned int rd = (ICODE16() & 0xf);
    unsigned int ld = (ICODE16() >> 8) & 0x3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_sub_src_dst_ub;
    INSTR->proc();
}

/**
 *************************************************************************
 * \fn void rx_sub_src_dst_nub(void)
 * Subtract an indirectly addressed B,W,L,UW with displacement from Rd
 * v0
 *************************************************************************
 */
static void
rx_sub_src_dst_nub(void)
{
    unsigned int ld = INSTR->arg1;
    unsigned int mi = INSTR->arg2;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = *INSTR->pRs;
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;
        default:
        case 3:
            fprintf(stderr, "SUB: RS variant should not exist\n");
            RX_REG_PC += 3;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd - Src;
    *INSTR->pRd = result;
    sub_flags(Rd, Src, result);
}

void
rx_setup_sub_src_dst_nub(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = (ICODE24() & 0xf);
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int mi = (ICODE24() >> 14) & 0x3;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->arg2 = mi;
    INSTR->proc = rx_sub_src_dst_nub;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_sub_src_src2_dst(void)
 * Subtract Src2 - Src and write to Rd.
 * v0
 ********************************************************************
 */
static void
rx_sub_src_src2_dst(void)
{
    uint32_t Src, Src2, result;
    Src = *INSTR->pRs;
    Src2 = *INSTR->pRs2;
    result = Src2 - Src;
    *INSTR->pRd = result;
    sub_flags(Src2, Src, result);
    RX_REG_PC += 3;
}

void
rx_setup_sub_src_src2_dst(void)
{
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = (ICODE24() & 0xf);
    unsigned int rd = (ICODE24() >> 8) & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_sub_src_src2_dst;
    INSTR->proc();
}

/**
 **************************************************************
 * \fn void rx_suntil(void)
 * Search for a string.
 * No effect on flags if R3 == 0
 **************************************************************
 */
static void
rx_suntil(void)
{
    unsigned int sz = INSTR->arg1;
    uint32_t R1, R2, R3;
    uint32_t tmp;
    R3 = RX_ReadReg(3);
    if (R3) {
        R1 = RX_ReadReg(1);
        switch (sz) {
            case 0:
                tmp = RX_Read8(R1);
                R1 += 1;
                break;
            case 1:
                tmp = RX_Read16(R1);
                R1 += 2;
                break;
            case 2:
                tmp = RX_Read32(R1);
                R1 += 4;
                break;
            default:
            case 3:
                fprintf(stderr, "Illegal sz in suntil\n");
                return;
        }
        RX_WriteReg(R1, 1);
        RX_WriteReg(R3 - 1, 3);
        R2 = RX_ReadReg(2);
        if (tmp >= R2) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        if (tmp == R2) {
            RX_REG_PC += 2;
            RX_REG_PSW |= PSW_Z;
        } else {
            RX_REG_PSW &= ~PSW_Z;
        }
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_setup_suntil(void)
{
    unsigned int sz = ICODE16() & 3;
    INSTR->arg1 = sz;
    INSTR->proc = rx_suntil;
    INSTR->proc();
}

/**
 ***********************************************************************
 * \fn void rx_swhile(void)
 * Search while unequal string.
 * v0
 ***********************************************************************
 */
static void
rx_swhile(void)
{
    unsigned int sz = INSTR->arg1;
    uint32_t R1, R2, R3;
    uint32_t tmp;
    R3 = RX_ReadReg(3);
    if (R3) {
        R1 = RX_ReadReg(1);
        switch (sz) {
            case 0:
                tmp = RX_Read8(R1);
                R1 += 1;
                break;
            case 1:
                tmp = RX_Read16(R1);
                R1 += 2;
                break;
            case 2:
                tmp = RX_Read32(R1);
                R1 += 4;
                break;
            default:
            case 3:
                fprintf(stderr, "Illegal sz in swhile\n");
                return;
        }
        RX_WriteReg(R1, 1);
        RX_WriteReg(R3 - 1, 3);
        R2 = RX_ReadReg(2);
        if (tmp >= R2) {
            RX_REG_PSW |= PSW_C;
        } else {
            RX_REG_PSW &= ~PSW_C;
        }
        if (tmp != R2) {
            RX_REG_PC += 2;
            RX_REG_PSW &= ~PSW_Z;
        } else {
            RX_REG_PSW |= PSW_Z;
        }
    } else {
        RX_REG_PC += 2;
    }
}

void
rx_setup_swhile(void)
{
    unsigned int sz = ICODE16() & 3;
    INSTR->arg1 = sz;
    INSTR->proc = rx_swhile;
    INSTR->proc();
}

/**
 **********************************************************************
 * \fn void rx_tst_imm_rs(void)
 * set flags according to logical and of Src and Src2
 * (1) And with a sign extended immediate.
 * v0
 **********************************************************************
 */
void
rx_tst_imm32_rs(void)
{
    uint32_t Rs2, result;
    uint32_t Src;
    Src = RX_Read32(RX_REG_PC + 3);
    RX_REG_PC += 7;
    Rs2 = *INSTR->pRs2;
    result = Rs2 & Src;
    and_flags(result);
}

void
rx_tst_simm8_rs(void)
{
    uint32_t Rs2, result;
    uint32_t Src;
    Src = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 3);
    RX_REG_PC += 4;
    Rs2 = *INSTR->pRs2;
    result = Rs2 & Src;
    and_flags(result);
}

void
rx_tst_simm16_rs(void)
{
    uint32_t Rs2, result;
    uint32_t Src;
    Src = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 3);
    RX_REG_PC += 5;
    Rs2 = *INSTR->pRs2;
    result = Rs2 & Src;
    and_flags(result);
}

void
rx_tst_simm24_rs(void)
{
    uint32_t Rs2, result;
    uint32_t Src;
    Src = RX_Read24(RX_REG_PC + 3);
    Src = ((int32_t) (Src << 8)) >> 8;
    RX_REG_PC += 6;
    Rs2 = *INSTR->pRs2;
    result = Rs2 & Src;
    and_flags(result);
}

void
rx_setup_tst_imm_rs(void)
{
    unsigned int rs2 = (ICODE24() & 0xf);
    unsigned int li = (ICODE24() >> 10) & 3;
    INSTR->pRs2 = RX_RegP(rs2);
    switch (li) {
        case 0:
            INSTR->proc = rx_tst_imm32_rs;
            break;
        case 1:
            INSTR->proc = rx_tst_simm8_rs;
            break;
        case 2:
            INSTR->proc = rx_tst_simm16_rs;
            break;
        case 3:
            INSTR->proc = rx_tst_simm24_rs;
            break;
    }
    INSTR->proc();
}

/**
 *************************************************************************
 * Set flags from logical and of Src with Src2.
 * (2) Src is a indirectly addressed unsigned byte or a register.
 *************************************************************************
 */
static void
rx_tst_src_src2_ub(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rs2, Src, result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 3;
            break;
    }
    Rs2 = *INSTR->pRs2;
    result = Rs2 & Src;
    and_flags(result);
}

void
rx_setup_tst_src_src2_ub(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rs2 = (ICODE24() & 0xf);
    INSTR->arg1 = ld;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRs2 = RX_RegP(rs2);
    INSTR->proc = rx_tst_src_src2_ub;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_tst_src_src2_nub(void)
 * Set flags on result of logical and of Src2 and Src.
 * (2b) Src is indirectly addressed B,W,L,UW with displacement.
 * v0
 *******************************************************************
 */
void
rx_tst_src_src2_nub(void)
{
    unsigned int ld = (ICODE32() >> 16) & 0x3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rs2 = (ICODE32() & 0xf);
    unsigned int mi = (ICODE32() >> 22) & 3;
    uint32_t Rs2, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "TST: RS variant should not exist\n");
            RX_REG_PC += 4;
            return;

    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rs2 = RX_ReadReg(rs2);
    result = Rs2 & Src;
    and_flags(result);
}

void
rx_wait(void)
{
    fprintf(stderr, "rx_wait not implemented\n");
    RX_REG_PC += 2;
}

/**
 ***********************************************************
 * \fn void rx_xchg_src_dst_ub(void)
 * Exchange the contents of src and destination
 * (1) Exchange Rd with indirectly addressed Byte or Rs
 * v0
 ***********************************************************
 */
static void
rx_xchg_src_dst_ub(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rd, Src;
    uint32_t dsp;
    Rd = *INSTR->pRd;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            RX_Write8(Rd, *INSTR->pRs);
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_Write8(Rd, *INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_Write8(Rd, *INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            Src = *INSTR->pRs;
            *INSTR->pRs = Rd;
            RX_REG_PC += 3;
            break;

    }
    *INSTR->pRd = Src;
}

void
rx_setup_xchg_src_dst_ub(void)
{
    unsigned int rd = ICODE24() & 0xf;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int ld = (ICODE24() >> 8) & 3;
    INSTR->arg1 = ld;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->proc = rx_xchg_src_dst_ub;
    INSTR->proc();
}

/**
 *******************************************************************
 * \fn void rx_xchg_src_dst_nub(void)
 * Exchange a Register value with an indirectly addressed B,W,L,UW
 * with displacement
 * v0
 *******************************************************************
 */
void
rx_xchg_src_dst_nub(void)
{
    unsigned int ld = (ICODE32() >> 16) & 3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    unsigned int mi = (ICODE32() >> 22) & 3;
    uint32_t Rd, Src;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "xchg: Rs variant should not exist\n");
            RX_REG_PC += 4;
            return;
    }
    Rd = RX_ReadReg(rd);
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            RX_Write8(Rd, memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            RX_Write16(Rd, memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            RX_Write32(Rd, memaddr);
            break;
        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            RX_Write16(Rd, memaddr);
            break;
    }
    RX_WriteReg(Src, rd);
}

/**
 ***************************************************************************
 * \fn void rx_xor_imm_dst(void)
 * (1) Exclusive or of a sign extended immediate and a destination Register.
 * v0
 ***************************************************************************
 */
static void
rx_xor_imm_dst(void)
{
    unsigned int li = INSTR->arg1;
    uint32_t Rd, result;
    uint32_t Src;
    switch (li) {
        case 0:
            Src = RX_Read32(RX_REG_PC + 3);
            RX_REG_PC += 7;
            break;
        case 1:
            Src = (int32_t) (int8_t) RX_Read8(RX_REG_PC + 3);
            RX_REG_PC += 4;
            break;
        case 2:
            Src = (int32_t) (int16_t) RX_Read16(RX_REG_PC + 3);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            Src = RX_ReadSimm24(RX_REG_PC + 3);
            RX_REG_PC += 6;
            break;
    }
    Rd = *INSTR->pRd;
    result = Rd ^ Src;
    *INSTR->pRd = result;
    xor_flags(result);
}

void
rx_setup_xor_imm_dst(void)
{
    unsigned int rd = (ICODE24() & 0xf);
    unsigned int li = (ICODE24() >> 10) & 3;
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = li;
    INSTR->proc = rx_xor_imm_dst;
    INSTR->proc();
}

/**
 ********************************************************************
 * \fn void rx_xor_src_dst_ub(void)
 * (2a) Exclusive or with an indirectly addressed unsigned byte or a
 * Register.
 * v0
 ********************************************************************
 */
static void
rx_xor_src_dst_ub(void)
{
    unsigned int ld = INSTR->arg1;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    switch (ld) {
        case 0:
            Src = RX_Read8(*INSTR->pRs);
            RX_REG_PC += 3;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 4;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 3);
            Src = RX_Read8(*INSTR->pRs + dsp);
            RX_REG_PC += 5;
            break;

        default:
        case 3:
            Src = *INSTR->pRs;
            RX_REG_PC += 3;
            break;

    }
    Rd = *INSTR->pRd;
    result = Rd ^ Src;
    *INSTR->pRd = result;
    xor_flags(result);
}

void
rx_setup_xor_src_dst_ub(void)
{
    unsigned int ld = (ICODE24() >> 8) & 0x3;
    unsigned int rs = (ICODE24() >> 4) & 0xf;
    unsigned int rd = ICODE24() & 0xf;
    INSTR->pRs = RX_RegP(rs);
    INSTR->pRd = RX_RegP(rd);
    INSTR->arg1 = ld;
    INSTR->proc = rx_xor_src_dst_ub;
    INSTR->proc();
}

/**
 ****************************************************************************
 * Exclusive or with an indirectly addressed B,W,L,UW with displacement.
 * \fn void rx_xor_src_dst_nub(void)
 * v0
 ****************************************************************************
 */
void
rx_xor_src_dst_nub(void)
{
    unsigned int ld = (ICODE32() >> 16) & 3;
    unsigned int rs = (ICODE32() >> 4) & 0xf;
    unsigned int rd = (ICODE32() & 0xf);
    unsigned int mi = (ICODE32() >> 22) & 3;
    uint32_t Rd, Src, result;
    uint32_t dsp;
    uint32_t memaddr;
    memaddr = RX_ReadReg(rs);
    switch (ld) {
        case 0:
            dsp = 0;
            RX_REG_PC += 4;
            break;
        case 1:
            dsp = RX_Read8(RX_REG_PC + 4);
            RX_REG_PC += 5;
            break;
        case 2:
            dsp = RX_Read16(RX_REG_PC + 4);
            RX_REG_PC += 6;
            break;

        default:
        case 3:
            fprintf(stderr, "Xor: Rs variant should not exist\n");
            RX_REG_PC += 4;
            return;
    }
    switch (mi) {
        case 0:                /* memex B */
            memaddr += dsp;
            Src = (int32_t) (int8_t) RX_Read8(memaddr);
            break;
        case 1:                /* memex W */
            memaddr += (dsp << 1);
            Src = (int32_t) (int16_t) RX_Read16(memaddr);
            break;
        case 2:                /* memex L */
            memaddr += (dsp << 2);
            Src = RX_Read32(memaddr);
            break;

        default:
        case 3:                /* memex UW */
            memaddr += (dsp << 1);
            Src = RX_Read16(memaddr);
            break;
    }
    Rd = RX_ReadReg(rd);
    result = Rd ^ Src;
    xor_flags(result);
    RX_WriteReg(result, rd);
}

void
rx_und(void)
{
    RX_Break();
    RX_Exception(0xffffffdc);
}

void
RXInstructions_Init(void)
{
    init_condition_map();
}
