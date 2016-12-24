/*
 ************************************************************************************************
 * RX-Flash
 *      Emulation of the internal flash of Renesas RX  series
 *
 * Status: Error handling is bad, lock bits not implemented 
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
 ************************************************************************************************
 */

#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "flash_rx.h"
#include "configfile.h"
#include "cycletimer.h"
#include "bus.h"
#include "signode.h"
#include "diskimage.h"
#include "sgstring.h"
#include "clock.h"

#define REG_FMODR(base)		(0x007FC402)
#define		FMODR_FRDMD	(1 << 4)
#define REG_FASTAT(base)	(0x007FC410)
#define		FASTAT_DFLWPE	(1 << 0)
#define		FASTAT_DFLRPE	(1 << 1)
#define		FASTAT_DFLAE	(1 << 3)
#define 	FASTAT_CMDLK	(1 << 4)
#define		FASTAT_ROMAE	(1 << 7)
#define REG_FAEINT(base)	(0x007FC411)
#define		FAEINT_DFLWPEIE	(1 << 0)
#define		FAEINT_DFLRPEIE	(1 << 1)
#define		FAEINT_DFLAEIE	(1 << 3)
#define 	FAEINT_CMDLKIE	(1 << 4)
#define		FAEINT_ROMAEIE	(1 << 7)
#define REG_FRDYIE(base)	(0x007FC412)
#define		FRDYIE_FRDYIE	(1 << 0)

#define	REG_DFLRE0(base)	(0x007FC440)
#define		DFLRE0_DBRE00		(1 << 0)
#define		DFLRE0_DREB01		(1 << 1)
#define		DFLRE0_DREB02		(1 << 2)
#define		DFLRE0_DREB03		(1 << 3)
#define		DFLRE0_DREB04		(1 << 4)
#define		DFLRE0_DREB05		(1 << 5)
#define		DFLRE0_DREB06		(1 << 6)
#define		DFLRE0_DREB07		(1 << 7)
#define		DFLRE0_KEY_MSK		(0xff << 8)
#define		DFLRE0_KEY_SHIFT	(8)
#define	REG_DFLRE1(base)	(0x007FC442)
#define		DFLRE1_DBRE08		(1 << 0)
#define		DFLRE1_DREB09		(1 << 1)
#define		DFLRE1_DREB10		(1 << 2)
#define		DFLRE1_DREB11		(1 << 3)
#define		DFLRE1_DREB12		(1 << 4)
#define		DFLRE1_DREB13		(1 << 5)
#define		DFLRE1_DREB14		(1 << 6)
#define		DFLRE1_DREB15		(1 << 7)
#define		DFLRE1_KEY_MSK		(0xff << 8)
#define		DFLRE1_KEY_SHIFT	(8)
#define REG_DFLWE0(base)	(0x007FC450)
#define		DFLWE0_DBWE00		(1 << 0)
#define		DFLWE0_DBWE01		(1 << 1)
#define		DFLWE0_DBWE02		(1 << 2)
#define		DFLWE0_DBWE03		(1 << 3)
#define		DFLWE0_DBWE04		(1 << 4)
#define		DFLWE0_DBWE05		(1 << 5)
#define		DFLWE0_DBWE06		(1 << 6)
#define		DFLWE0_DBWE07		(1 << 7)
#define REG_DFLWE1(base)	(0x007FC452)
#define		DFLWE1_DBWE08		(1 << 0)
#define		DFLWE1_DBWE09		(1 << 1)
#define		DFLWE1_DBWE10		(1 << 2)
#define		DFLWE1_DBWE11		(1 << 3)
#define		DFLWE1_DBWE12		(1 << 4)
#define		DFLWE1_DBWE13		(1 << 5)
#define		DFLWE1_DBWE14		(1 << 6)
#define		DFLWE1_DBWE15		(1 << 7)

#define REG_FCURAME(base) 	(0x007FC454)
#define		FCURAME_FCRME		(1 << 0)
#define		FCURAME_KEY_SHIFT	(8)
#define		FCURAME_KEY_MSK		(0xff << 8)

#define REG_FSTATR0(base)	(0x007FFFB0)
#define 	FSTATR0_PRGSPD		(1 << 0)
#define		FSTATR0_ERSSPD		(1 << 1)
#define		FSTATR0_SUSRDY		(1 << 3)
#define		FSTATR0_PRGERR		(1 << 4)
#define 	FSTATR0_ERSERR		(1 << 5)
#define		FSTATR0_ILGLERR		(1 << 6)
#define		FSTATR0_FRDY		(1 << 7)

#define REG_FSTATR1(base)	(0x007FFFB1)
#define		FSTATR1_FLOCKST		(1 << 4)
#define		FSTATR1_FCUERR		(1 << 7)

#define REG_FENTRYR(base)	(0x007FFFB2)
#define		FENTRYR_FENTRY0		(1 << 0)
#define		FENTRYR_FENTRYD		(1 << 7)
#define		FENTRYR_FEKEY_SHIFT	(8)
#define		FENTRYR_FEKEY_MSK	(0xff << 8)

#define REG_FPROTR(base)	(0x007FFFB4)
#define		FPROTR_FPROTCN		(1 <<0)
#define		FPROTR_FPKEY_SHIFT	(8)
#define		FPROTR_FPKEY_MSK	(0xff << 8)

#define REG_FRESETR(base)	(0x007FFFB6)
#define		FRESETR_FRESET		(1 << 0)
#define		FRESETR_KEY_SHIFT	(8)
#define		FRESETR_KEY_MSK		(0xff << 8)

#define REG_FCMDR(base)		(0x007FFFBA)
#define		FCMDR_PCMDR_SHIFT	(0)
#define		FCMDR_PCMDR_MSK		(0xff)
#define		FCMDR_CMDR_SHIFT	(8)
#define		FCMDR_CMDR_MSK		(0xff << 8)

#define REG_FCPSR(base)		(0x007FFFC8)
#define		FCPSR_ESUSPMD		(1 << 0)

#define	REG_DFLBCCNT(base)	(0x007FFFCA)
#define		DFLBCCNT_BCSIZE		(1 << 0)
#define		DFLBCCNT_BCADR_MSK	(0xff << 3)
#define		DFLBCCNT_BCADR_SHIFT	(3)

#define REG_DFLBCSTAT(base)	(0x007FFFCE)
#define		DFLBCSTAT_BCST		(1 << 0)

#define REG_FPESTAT(base)	(0x007FFFCC)
#define		FPESTAT_PEERRST_SHIFT	(0)
#define		FPESTAT_PEERRST_MSK	(0xff)

#define REG_PCKAR(base)		(0x007FFFE8)
#define		PCKAR_PCKA_SHIFT	(0)
#define		PCKAR_PCKA_MSK		(0xff)
#define REG_FWEPROR(base)	(0x0008C289)
#define		FWEPROR_FLWE_SHIFT	(0)
#define		FWEPROR_FLWE_MSK	(3)

#define FCU_RAM_SIZE	(8192)
#define FCU_ROM_SIZE 	FCU_RAM_SIZE
#define FCU_RAM_TOP	0x007F8000
#define FCU_ROM_TOP	0xFEFFE000
#define DATA_FLASH_BASE	0x00100000
#define ROM_BASE_WRITE(rxflash) ((rxflash)->rom_base & 0x00ffffff)

#define RCMD_TRANSNORM		(0xff)
#define RCMD_TRANSSTATUSREAD	(0x70)
#define RCMD_TRANSLOCKREAD	(0x71)
#define RCMD_SETPCK		(0xe9)
#define RCMD_PROGRAMM		(0xe8)
#define RCMD_BLOCKERASE		(0x20)
#define RCMD_PESUSPEND		(0xb0)
#define RCMD_PERESUME		(0xd0)
#define RCMD_CLEARSTATUS	(0x50)
#define RCMD_READLOCKBIT2	(0x71)
#define RCMD_PGMLOCKBIT		(0x77)
#define DCMD_BLANKCHECK		(0x71)

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

typedef struct RXFlash {
	/* State space from two variables */
	BusDevice bdev;
	Clock_t *clkIn;
	DiskImage *rom_image;
	DiskImage *data_image;
	DiskImage *blank_image;
	DiskImage *fcu_rom_image;
	uint8_t *rom_mem;
	uint8_t *data_mem;
	uint8_t *blank_mem;

	uint8_t *fcu_ram;
	uint8_t *fcu_rom;
	uint32_t dfsize;
	uint32_t rom_base;
	uint32_t romsize;
	uint32_t blank_size;
	bool fcu_ram_verified;

	bool dbgMode;
	uint8_t regFMODR;
	uint8_t regFASTAT;
	uint8_t regFAEINT;
	uint8_t regFRDYIE;
	uint16_t regDFLRE;
	uint16_t regDFLWE;
	uint16_t regFCURAME;
	uint8_t regFSTATR0;
	uint8_t regFSTATR1;
	uint16_t regFENTRYR;
	uint16_t regFPROTR;
	uint16_t regFRESETR;
	uint16_t regFCMDR;
	uint16_t regFCPSR;
	uint16_t regFPESTAT;
	uint16_t regDFLBCCNT;
	uint16_t regDFLBCSTAT;
	uint16_t regPCKAR;
	uint8_t regFWEPROR;
	/* Rom FCU command state machine */
	uint16_t stateCmd;
	uint16_t stateCmdCycle;	/* Starts with 1 because same number as in manual */
	/*  Only for detection of errors if RA/BA changes during the command */
	uint32_t stateRA;
	uint32_t stateBA;
	uint32_t pgmLen;
	/* Pixel clock set command copies regPCKAR to fcuPCKAR */
	uint16_t fcuPCKAR;
	CycleCounter_t busy_until;

	uint8_t bufPGM[256];
	/* Timing Parameters */
	uint32_t tDP8;
	uint32_t tDP128;
	uint32_t tDE2K;
	uint32_t tDBC8;
	uint32_t tDBC2K;
	uint32_t tDSPD;
	uint32_t tDSESD1;
	uint32_t tDSESD2;
	uint32_t tDSEED;

	uint32_t tP256;
	uint32_t tP4K;
	uint32_t tP16K;
	uint32_t tE4K;
	uint32_t tE16K;
	uint32_t tSPD;
	uint32_t tSESD1;
	uint32_t tSESD2;
	uint32_t tSEED;

	uint32_t tSETPCKAR;
} RXFlash;

static void
make_busy(RXFlash * rf, uint32_t useconds)
{
	rf->busy_until = CycleCounter_Get() + MicrosecondsToCycles(useconds);
	rf->regFSTATR0 &= ~FSTATR0_FRDY;
}

static bool
check_busy(RXFlash * rf)
{
	if (CycleCounter_Get() < rf->busy_until) {
		return true;
	} else {
		return false;
	}
}

static uint32_t
fmodr_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFMODR;
}

static void
fmodr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	rf->regFMODR = value & FMODR_FRDMD;
}

/**
 *********************************************************************************
 * Bit 0: DFLWPE Data Flash Programming/Erasure protection violation
 * Bit 1: DFLRPE Data Flash Read Protection Violation.
 * Bit 3: DFLAE Data flash access violation	
 * Bit 4: CMDLK	FCU Command lock 1 = FCU is in the command locked state.
 * Bit 7: ROMAE	Rom Access Violation.
 *********************************************************************************
 */
static uint32_t
fastat_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
fastat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

/**
 ****************************************************************************
 * FAEINT Flash Access Error Interrupt Enable Register
 * Bit 7: ROMAEIE ROM Access Violation Interrupt Enable
 * Bit 4: CMDLKIE FCU Command Lock Interrupt Enable
 * Bit 3: DFLAEIE Data Flash Access Violation Interrupt
 * Bit 1: DFLRPEIE Data Flash Read Protection Violation Interrupt Enable
 * Bit 0: DFLWPEIE Data Flash Write Protection Violation Interrupt Enable
 ***************************************************************************
 */

static uint32_t
faeint_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
faeint_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

/**
 **********************************************************************
 * FRDYIE Flash Ready Interrupt Enable Register
 * BIT 0: Flash Ready Interrupt Enable
 **********************************************************************
 */
static uint32_t
frdyie_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFRDYIE;
}

static void
frdyie_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

/*
 ****************************************************************************
 * Data Flash Read Enable Register 0 (Datablock 00 - 07)
 * Bit 0 - 7: DBRE0x: Data Block 0x Block Read Enable
 * Bit 8 - 16: Key code. Must be 0x2D on write
 ****************************************************************************
 */

static uint32_t
dflre0_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regDFLRE & 0xff;
}

static void
dflre0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	if ((value & 0xFF00) == 0x2D00) {
		rf->regDFLRE = value & 0xff;
	} else {
		fprintf(stderr, "DFLRE0 write: Wrong Key %04x\n", value);
	}
}

/**
 *****************************************************************
 * Data Flash Read Enable 1
 * Bit 0 - 7: Data block 08 - 15 Read enable
 * Bit 8 - 16: Key code. Must be 0xD2 on write
 *****************************************************************
 */
static uint32_t
dflre1_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return (rf->regDFLRE >> 8) & 0xff;
}

static void
dflre1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	if ((value & 0xFF00) == 0xD200) {
		rf->regDFLRE = (rf->regDFLRE & 0xff00) | ((value & 0xff) << 8);
	} else {
		fprintf(stderr, "DFLRE1 write: Wrong Key %04x\n", value);
	}
}

/**
 ****************************************************************
 * Data flash write enable 0: Block 0 - 7
 * Bit 0 - 7: Data Block Programming/Erasure enable Block 00 - 07
 * Bit 8 - 15: Key code. Must Be 0x1E on write
 ****************************************************************
 */
static uint32_t
dflwe0_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regDFLWE & 0xff;
}

static void
dflwe0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	if ((value & 0xFF00) == 0x1E00) {
		rf->regDFLWE = (rf->regDFLWE & 0xff00) | (value & 0xff);
	} else {
		fprintf(stderr, "DFLRE1 write: Wrong Key %04x\n", value);
	}
}

/**
 *************************************************************************
 * Data flash write enable 1 
 * Bit 0 -7: Write enable Data block 08 - 15
 * Bit 8 - 15: Key code. Must Be 0xE1 on write
 *************************************************************************
 */

static uint32_t
dflwe1_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return (rf->regDFLWE >> 8) & 0xff;
}

static void
dflwe1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	if ((value & 0xFF00) == 0xE100) {
		rf->regDFLWE = (rf->regDFLWE & 0xff) | ((value & 0xff) << 8);
	} else {
		fprintf(stderr, "DFLWE1 write: Wrong Key %04x\n", value);
	}
}

static uint32_t
fcurame_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFCURAME;
}

static void
fcurame_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rxflash = clientData;
	if ((value & FCURAME_KEY_MSK) == 0xC400) {
		rxflash->regFCURAME = value & 1;
		Mem_AreaUpdateMappings(&rxflash->bdev);
		//fprintf(stderr,"Updated FCURAME to %02x\n",rxflash->regFCURAME);
		//exit(1);
	} else {
		fprintf(stderr, "RX-Flash: Wrong key writing to FCURAME: %04x\n", value);
	}
}

/**
 ****************************************************************
 * FSTATR0 Flash Status Register 0
 * Bit 7: FRDY Flash Ready
 * Bit 6: ILGERR Illegal Command Error
 * Bit 5: ERSERR Erasure Error
 * Bit 4: PRGERR Programming Error
 * Bit 4: SUSRDY Suspend Ready
 * Bit 1: ERSSPD Erase Suspend Status
 * Bit 0: PRGSPD Programming Suspend Status
 ****************************************************************
 */
static uint32_t
fstatr0_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	uint32_t retval = rf->regFSTATR0;
	if (check_busy(rf) == false) {
		retval |= FSTATR0_FRDY;
	}
	return retval;
}

static void
fstatr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s Writing to Readonly register\n", __func__);
}

/*
 ************************************************************************
 * Flash Status Register 1 FSTATR1
 * Bit 7: FCUERR FCU Error 
 * Bit 4: FLOCKST Lock Bit Status 0: Protected 1: Not protected
 ************************************************************************
 */
static uint32_t
fstatr1_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFSTATR1;
}

static void
fstatr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s Writing to Readonly register\n", __func__);
}

/*
 *****************************************************************
 * Flash P/E Mode Entry Register. 
 * Bit 7: FENTRYD Data Flash Programm/Erase mode
 * Bit 0: FENTRY0 ROM P/E mode 0: 0 = read mode 1 = P/E mode
 *****************************************************************
 */
static uint32_t
fentryr_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFENTRYR;
}

static void
fentryr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	if ((value & 0xff00) == 0xaa00) {
		uint16_t diff = rf->regFENTRYR ^ value;
		rf->regFENTRYR = value & 0x81;
		if (diff & 0x81) {
			Mem_AreaUpdateMappings(&rf->bdev);
		}
	} else {
		fprintf(stderr, "%s Wrong Key 0x%04x\n", __func__, value);
	}
}

/*
 *******************************************************************************
 * FPROTR Flash Protection Register
 * Bit 0 FPROTCN Lock bit Protection Cancel. 0: enable Prot. 1: disable Prot 
 * Bit 8 - 15: KEY must be 0x55
 *******************************************************************************
 */

static uint32_t
fprotr_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFPROTR;
}

static void
fprotr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	if ((value & 0xff00) == 0x5500) {
		rf->regFPROTR = value & 1;
	} else {
		fprintf(stderr, "%s wrong key 0x%04x\n", __func__, value);
	}
}

/**
 *************************************************************************
 * Flash Reset Register
 * Bit 0: FRESET
 * BIT 8-15 FRKEY must be 0xCC on write
 *************************************************************************
 */
static uint32_t
fresetr_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFRESETR;
}

static void
fresetr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	if ((value & 0xff00) == 0xCC00) {
		rf->regFRESETR = value & 1;
		rf->regFPESTAT = 0;
		rf->regFCMDR = 0;
		rf->regFPROTR = 0;
		rf->regFENTRYR = 0;
		rf->regFRDYIE = 0;
		rf->regFSTATR1 = 0;
		rf->regFSTATR0 = 0x80;
		rf->regFCURAME = 0;
		rf->regDFLBCSTAT = 0;
		rf->regDFLBCCNT = 0;
		rf->regPCKAR = 0;
		Mem_AreaUpdateMappings(&rf->bdev);
		fprintf(stderr, "%s incomplete implementation\n", __func__);
	} else {
		fprintf(stderr, "%s wrong key 0x%04x\n", __func__, value);
	}
}

/**
 **************************************************************************
 * FCU Command Register FCMDR
 * Bit 0 - 7: PCMDR Precommand
 * Bit 8 - 15: CMDR Last command received by the FCU
 **************************************************************************
 */
static uint32_t
fcmd_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
fcmd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

/**
 **************************************************************************
 * FCU Processing Switching Register (FCPSR)
 * Bit 0: ESUSPMD Erase suspend Mode
 **************************************************************************
 */
static uint32_t
fcpsr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
	return 0;
}

static void
fcpsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not implemented\n", __func__);
}

/**
 *******************************************************************************
 * Flash P/E Status Register (FPESTAT)
 * Bit 0 - 7: PEERRST P/E Error Status
 * 	      0x01: Programming locked error
 *	      0x02: Other Programming error
 *	      0x11: Erase locked error 
 *	      0x12: Other erase error
 *******************************************************************************
 */
static uint32_t
fpestat_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFPESTAT;
}

static void
fpestat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not writable\n", __func__);
}

/**
 *******************************************************************************
 * DFLBCCNT Data Flash Blank Check Control Register
 * Bit 0: BCSIZE 0 = area of 8b is blank checked 1 = area of 2k is blank checked
 * Bit 3-10 Blank check start addr.
 *******************************************************************************
 */
static uint32_t
dflbccnt_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regDFLBCCNT;
}

static void
dlfbccnt_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	rf->regDFLBCCNT = value & (DFLBCCNT_BCSIZE | DFLBCCNT_BCADR_MSK);
}

/*
 ***********************************************************************
 * Data flash blank check status register. 
 * Holds the result of the blank check operation
 * Bit 0: Blank check status
 ***********************************************************************
 */

static uint32_t
dflbcstat_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regDFLBCSTAT;
}

static void
dflbcstat_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "%s not writable\n", __func__);
}

/**
 ****************************************************************
 * Peripheral Clock Notification Register.
 * Holds the Peripheral Clock in MHz
 ****************************************************************
 */
static uint32_t
pckar_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regPCKAR;
}

static void
pckar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	rf->regPCKAR = value & 0xff;
}

static inline uint32_t
RomBlockSize(RXFlash * rf, uint32_t offset)
{
	if (offset < (rf->romsize - 32768)) {
		return 16384;
	} else {
		return 4096;
	}
}

/**
 ****************************************************************
 * Flash Write/Erase Protection Register
 * Bit 0 - 1: FLWE 01 = flash Write/erase enabled, else disabled
 ****************************************************************
 */
static uint32_t
fwepror_read(void *clientData, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	return rf->regFWEPROR;
}

static void
fwepror_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	RXFlash *rf = clientData;
	rf->regFWEPROR = value & 3;
}

static void
FCU_ExecRomPGM(RXFlash * rf)
{
	unsigned int i;
	uint32_t offset = (rf->stateRA & ~0xff) - ROM_BASE_WRITE(rf);
	uint32_t pgmbytes = rf->pgmLen << 1;
	uint32_t by, bi;
	if (!rf->fcuPCKAR) {
		fprintf(stderr, "\nRXFlash: FCU Peripheral clock not configured\n");
		exit(1);
	}
	dbgprintf("Flash address is %08x, offset %08x %d bytes\n", rf->stateRA, offset, pgmbytes);

	if ((offset + 256) <= rf->romsize) {
		if (rf->pgmLen != 128) {
			fprintf(stderr, "Wrong pgm len for ROM PGM: %u\n", rf->pgmLen);
			exit(1);
		}
		for (i = 0; i < 256; i++) {
			if (rf->rom_mem[offset + i] != 0xff) {
				fprintf(stderr, "RXFlash: Programming nonempty ROM\n");
				break;
			}
		}
		for (i = 0; i < 256; i++) {
			rf->rom_mem[offset + i] &= rf->bufPGM[i];
		}
		make_busy(rf, rf->tP256);
		return;
	}
	offset = (rf->stateRA & ~(pgmbytes - 1)) - DATA_FLASH_BASE;
	if (offset + pgmbytes <= rf->dfsize) {
		if (!((rf->regDFLWE >> (offset >> 11)) & 1)) {
			fprintf(stderr, "DFlash is write protected: offset 0x%08x, DFLWE 0x%04x\n",
				offset, rf->regDFLWE);
			rf->regFSTATR0 |= FSTATR0_PRGERR;
			rf->regFASTAT |= FASTAT_CMDLK;
			make_busy(rf, 0);
			exit(1);
		}
		bi = offset >> 3;
		by = bi >> 3;
		bi = bi & 7;
		dbgprintf("Programming %d bytes, offset %08x, by %d, bit %d\n", pgmbytes, offset,
			  by, bi);
		if (by >= (rf->blank_size)) {
			fprintf(stderr, "Bug in unblank , by %u, dfsize %u\n", by, rf->dfsize);
			exit(1);
		}
		if ((rf->blank_mem[by] >> bi) & 1) {
			for (i = 0; i < pgmbytes; i++) {
				rf->data_mem[offset + i] = rf->bufPGM[i];
			}
			rf->blank_mem[by] &= ~(1 << bi);
		} else {
			fprintf(stderr, "Bug, memory at 0x%08x is not blank\n", offset);
			exit(1);
			/* Probably some error ? */
		}
		if (pgmbytes == 8) {
			make_busy(rf, rf->tDP8);
		} else {
			make_busy(rf, rf->tDP128);
		}
	} else {
		rf->regFSTATR0 |= FSTATR0_PRGERR;
		rf->regFASTAT |= FASTAT_CMDLK;
		make_busy(rf, 0);
		fprintf(stderr,
			"Flash address 0x%08x is outside of flash, offset %08x pgmbytes %u dfbase %08x\n",
			rf->stateRA, offset, pgmbytes, DATA_FLASH_BASE);
		exit(1);
	}
	return;
}

/**
 ***************************************************************
 * Tell the FCU about the PCLK.
 ***************************************************************
 */
static void
FCU_ExecRomSetPCK(RXFlash * rf)
{
	uint32_t clkfreq;
	clkfreq = (Clock_Freq(rf->clkIn) + 500000) / 1000000;
	rf->fcuPCKAR = rf->regPCKAR;
	make_busy(rf, rf->tSETPCKAR);
	rf->stateCmdCycle = 1;
	if ((rf->regPCKAR < 8) || (rf->regPCKAR > 50)) {
		fprintf(stderr, "PCKAR is out of Range (8-50): %u Mhz\n", rf->fcuPCKAR);
		rf->regFSTATR0 |= FSTATR0_ILGLERR;
		rf->regFASTAT |= FASTAT_CMDLK;
		exit(1);
	}
	if (rf->regPCKAR != clkfreq) {
		static int maxwarn = 3;
		if (maxwarn > 0) {
			fprintf(stderr, "Warning FCU freq is %u MHz but PCLK is %u MHz\n",
				rf->regPCKAR, clkfreq);
			sleep(1);
			maxwarn--;
		}
	}
	dbgprintf("PCKAR is %u\n", rf->fcuPCKAR);
	return;
}

static void
FCU_ExecRomErase(RXFlash * rf)
{
	uint32_t offset;
	uint32_t by;
	uint32_t i;
	uint32_t blockSize;
	if (!rf->fcuPCKAR) {
		fprintf(stderr, "\nRXFlash: FCU Peripheral clock not configured\n");
		exit(1);
	}
	offset = rf->stateBA - ROM_BASE_WRITE(rf);
	if (offset < rf->romsize) {
		blockSize = RomBlockSize(rf, offset);
		offset = offset & ~(blockSize - 1);
		memset(rf->rom_mem + offset, 0xff, blockSize);
		if (blockSize == 4096) {
			make_busy(rf, rf->tE4K);
		} else {
			make_busy(rf, rf->tE16K);
		}
		return;
	}
	offset = rf->stateBA - DATA_FLASH_BASE;
	if (offset < rf->dfsize) {
		if (!((rf->regDFLWE >> (offset >> 11)) & 1)) {
			fprintf(stderr, "DFlash is erase protected: offset 0x%08x, DFLWE 0x%04x\n",
				offset, rf->regDFLWE);
			rf->regFSTATR0 |= FSTATR0_PRGERR;
			rf->regFASTAT |= FASTAT_CMDLK;
			make_busy(rf, 0);
			exit(1);
			return;
		}
		blockSize = 2048;
		offset = offset & ~(blockSize - 1);
		/* Should be more random */
		for (i = 0; i < blockSize; i += 1 + (i & 3)) {
			rf->data_mem[i + offset] ^= (1 << (i & 7));
		}
		make_busy(rf, rf->tDE2K);
		/* Now update blank map */
		by = offset >> 6;
		if (by + (blockSize >> 6) > rf->blank_size) {
			fprintf(stderr, "Bug: Outside of blank_mem\n");
			exit(1);
		}
		memset(rf->blank_mem + by, 0xff, blockSize >> 6);
		//fprintf(stderr,"Data flash erase at offset %08x, BA %08x\n",offset,rf->stateBA);
		return;
	}
	fprintf(stderr, "Erase address outside of window error\n");
}

static void
FCU_ExecLockBitRead2(RXFlash * rf)
{
	fprintf(stderr, "Read lock bit2 not implemented\n");
	rf->stateCmdCycle = 1;
}

static void
FCU_ExecLKBitPGM(RXFlash * rf)
{
	fprintf(stderr, "PGM lock bit not implemented\n");
	make_busy(rf, 0);
	rf->stateCmdCycle = 1;
}

static void
FCU_ExecBlankCheck(RXFlash * rf)
{
	uint32_t addr = rf->stateBA & (~UINT32_C(0x7ff) & (rf->dfsize - 1));
	uint32_t by, bi;
	uint32_t i;
	uint32_t bcsize;	/* Is Nr of BC Bitmap bits */
	if (!rf->fcuPCKAR) {
		fprintf(stderr, "\nRXFlash: FCU Peripheral clock not configured\n");
		exit(1);
	}
	dbgprintf("ExecBlankCheck BA %08x, bit %u\n", rf->stateBA,
		  rf->regDFLBCCNT >> DFLBCCNT_BCADR_SHIFT);

	if (!(rf->regFMODR & FMODR_FRDMD)) {
		fprintf(stderr, "DFlash Error: Blank check with FRDMD bit in FMODR == 0\n");
		rf->stateCmdCycle = 1;
		rf->regFSTATR0 |= FSTATR0_ILGLERR;	/* ILLGERR is just a guess */
		make_busy(rf, 0);
		exit(1);
	}
	addr >>= 3;
	if (rf->regDFLBCCNT & DFLBCCNT_BCSIZE) {
		bcsize = 2048 >> 3;
	} else {
		bcsize = 8 >> 3;
		addr |= (rf->regDFLBCCNT >> DFLBCCNT_BCADR_SHIFT);
	}
	by = addr >> 3;
	bi = addr & 7;
	if ((by + ((bcsize + 7) >> 3)) > rf->blank_size) {
		fprintf(stderr, "BlankCheck: outside of BC bitmap %u, dfsize %u,bcsize %u\n", by,
			rf->dfsize, bcsize);
		exit(1);
	}
	dbgprintf("BlankCheck Bitmap: Byte %u,bit %u\n", by, bi);
	if (bcsize == 1) {
		if ((rf->blank_mem[by] >> bi) & 1) {
			rf->regDFLBCSTAT = 0;
		} else {
			rf->regDFLBCSTAT = DFLBCSTAT_BCST;
		}
		make_busy(rf, rf->tDBC8);
	} else {
		rf->regDFLBCSTAT = 0;
		for (i = 0; i < (bcsize >> 3); i += 8) {
			if (rf->blank_mem[by + i] != 0xff) {
				rf->regDFLBCSTAT = DFLBCSTAT_BCST;
			}
		}
		make_busy(rf, rf->tDBC2K);
	}
	rf->stateCmdCycle = 1;
}

static void
RXRom_SetPCKCmd(RXFlash * rf, uint32_t value, uint32_t mem_addr, int rqlen)
{
	dbgprintf("SetPCK val %04x addr %08x rqlen %d\n", value, mem_addr, rqlen);
	switch (rf->stateCmdCycle) {
	    case 2:
		    if ((value == 0x03) && (rqlen == 1)) {
			    rf->stateCmdCycle++;
		    } else {
			    fprintf(stderr, "RXFlash: ILLSEQ in %s cycle %u\n", __func__,
				    rf->stateCmdCycle);
			    exit(1);
		    }
		    break;

	    case 3:
	    case 4:
	    case 5:
		    if ((value == 0x0f0f) && (rqlen == 2)) {
			    rf->stateCmdCycle++;
		    } else {
			    fprintf(stderr, "RXFlash: ILLSEQ in %s cycle %u\n", __func__,
				    rf->stateCmdCycle);

			    sleep(1);
			    // goto command locked state
			    exit(1);
		    }
		    break;
	    case 6:
		    if ((value == 0xd0) && (rqlen == 1)) {
			    FCU_ExecRomSetPCK(rf);
		    } else {
			    fprintf(stderr, "RXFlash: ILLSEQ in %s cycle %u\n", __func__,
				    rf->stateCmdCycle);
			    exit(1);
		    }
		    break;
	    default:
		    fprintf(stderr, "RXFlash PCK Cmd bug: illegal cycle %d\n", rf->stateCmdCycle);
		    rf->stateCmdCycle = 1;
		    exit(1);
	}
}

static void
RXRom_PGMCmd(RXFlash * rf, uint32_t value, uint32_t mem_addr, int rqlen)
{
	int32_t buf_idx;
	dbgprintf("PGMCmd: addr %08x value %02x len %d, cycle %d\n", mem_addr, value, rqlen,
		  rf->stateCmdCycle);
	buf_idx = (rf->stateCmdCycle - 3) << 1;
	if ((buf_idx >= 0) && (buf_idx < sizeof(rf->bufPGM))) {
		rf->bufPGM[buf_idx] = value & 0xff;
		rf->bufPGM[buf_idx + 1] = (value >> 8) & 0xff;
	}
	/* Unclear if this is true: */
	if (rqlen == 2) {
		rf->stateRA = mem_addr;
	}
	switch (rf->stateCmdCycle) {
	    case 2:
		    if ((value == 0x80) && (rqlen == 1) &&
			((mem_addr & ~0xff) == (rf->stateRA & ~0xff))) {
			    rf->stateCmdCycle++;
			    rf->pgmLen = value;
		    } else if ((value == 0x40) && (rqlen == 1)) {
			    rf->stateCmdCycle++;
			    rf->pgmLen = value;
		    } else if ((value == 0x04) && (rqlen == 1)) {
			    rf->stateCmdCycle++;
			    rf->pgmLen = value;
		    } else {
			    fprintf(stderr, "RXFlash: ILLSEQ in %s\n", __func__);
			    exit(1);
		    }
		    break;

	    case 7:
		    if (rf->pgmLen > 4) {
			    rf->stateCmdCycle++;
			    break;
		    }
	    case 67:
		    if (rf->pgmLen > 64) {
			    rf->stateCmdCycle++;
			    break;
		    }
	    case 131:
		    if ((value == 0xd0) && (rqlen == 1)) {
			    FCU_ExecRomPGM(rf);
			    rf->stateCmdCycle = 1;
		    } else {
			    fprintf(stderr, "RXFlash: ILLSEQ in %s\n", __func__);
			    exit(1);
		    }
		    break;
	    default:
		    if (rqlen != 2) {
			    fprintf(stderr, "RXFlash: ILLSEQ in %s\n", __func__);
			    exit(1);
		    } else if (buf_idx < sizeof(rf->bufPGM)) {
			    rf->stateCmdCycle++;
		    } else {
			    fprintf(stderr, "RXFlash: Simulator bug in PGM index\n");
			    exit(1);
		    }
		    break;

	}
}

static void
RXRom_BLKEraseCmd(RXFlash * rf, uint32_t value, uint32_t mem_addr, int rqlen)
{
	if (rf->stateCmdCycle == 2) {
		if ((rqlen == 1) && (value == 0xd0)) {
			rf->stateBA = mem_addr;
			FCU_ExecRomErase(rf);
			rf->stateCmdCycle = 1;
		} else {
			fprintf(stderr, "RXFlash: ILLSEQ in %s\n", __func__);
			exit(1);
		}
	} else {
		fprintf(stderr, "RXFlash: Simulator bug in BLKEraseCmd\n");
		exit(1);
	}
}

static void
RXRom_LKBitRead2Cmd(RXFlash * rf, uint32_t value, uint32_t mem_addr, int rqlen)
{
	if (rf->stateCmdCycle == 2) {
		if ((rqlen == 1) && (value == 0xd0)) {
			rf->stateRA = mem_addr;
			FCU_ExecLockBitRead2(rf);
		} else {
			fprintf(stderr, "RXFlash: ILLSEQ in %s\n", __func__);
			exit(1);
		}
	} else {
		fprintf(stderr, "RXFlash: Simulator bug in LKBitRead2Cmd\n");
		exit(1);
	}

}

static void
RXRom_LKBitPGMCmd(RXFlash * rf, uint32_t value, uint32_t mem_addr, int rqlen)
{
	if (rf->stateCmdCycle == 2) {
		if ((rqlen == 1) && (value == 0xd0)) {
			rf->stateRA = mem_addr;
			FCU_ExecLKBitPGM(rf);
		} else {
			fprintf(stderr, "RXFlash: ILLSEQ in %s\n", __func__);
			exit(1);
		}
	} else {
		fprintf(stderr, "RXFlash: Simulator bug in LKBitPGMCmd\n");
		exit(1);
	}

}

static void
RXDFlash_BlankCheckCmd(RXFlash * rf, uint32_t value, uint32_t mem_addr, int rqlen)
{
	if (rf->stateCmdCycle == 2) {
		if ((rqlen == 1) && (value == 0xd0)) {
			rf->stateBA = mem_addr;
			FCU_ExecBlankCheck(rf);
		} else {
			fprintf(stderr, "RXFlash: ILLSEQ in %s\n", __func__);
			exit(1);
		}
	} else {
		fprintf(stderr, "RXFlash: Simulator bug in LKBitPGMCmd\n");
		exit(1);
	}
}

/*
 ***************************************************************************
 * Read an write of data flash when in P/E mode
 ***************************************************************************
 */
static uint32_t
RXFlash_RomRead(void *clientData, uint32_t mem_addr, int rqlen)
{
	fprintf(stderr, "IOH mode in %s not implemented\n", __func__);
	return 0;
}

static void
RXFlash_RomWrite(void *clientData, uint32_t value, uint32_t mem_addr, int rqlen)
{
	RXFlash *rf = clientData;
	if (rf->fcu_ram_verified == false) {
		if (memcmp(rf->fcu_ram, rf->fcu_rom, FCU_ROM_SIZE) == 0) {
			rf->fcu_ram_verified = true;
		} else {
			fprintf(stderr, "RX_Flash: Write CMD with wrong FCU RAM content\n");
			exit(1);
			return;
		}
	}
	if (check_busy(rf)) {
		fprintf(stderr, "RX_Flash: RomWrite FCU access while busy\n");
		exit(1);
	}
	if (rf->stateCmdCycle == 1) {
		if (rqlen != 1) {
			fprintf(stderr, "RXFlash Write CMD wrong access width %d\n", rqlen);
			return;
		}
		rf->stateRA = mem_addr;
		rf->regFSTATR0 &= ~FSTATR0_FRDY;
		switch (value) {
		    case RCMD_TRANSNORM:
			    break;
		    case RCMD_TRANSSTATUSREAD:
			    break;
#if 0
		    case RCMD_TRANSLOCKREAD:
			    break;
#endif
		    case RCMD_SETPCK:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;
		    case RCMD_PROGRAMM:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    rf->pgmLen = 0x80;	/* Assume maximum initially */
			    break;
		    case RCMD_BLOCKERASE:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;
		    case RCMD_PESUSPEND:
			    break;
		    case RCMD_PERESUME:
			    break;
		    case RCMD_CLEARSTATUS:
			    rf->regFSTATR0 = 0;
			    break;
		    case RCMD_READLOCKBIT2:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;
		    case RCMD_PGMLOCKBIT:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;
		    default:
			    fprintf(stderr, "Unknown cmd %02x error\n", rf->stateCmd);
			    /* Some error */
			    break;
		}
	} else {
		switch (rf->stateCmd) {
		    case RCMD_SETPCK:
			    RXRom_SetPCKCmd(rf, value, mem_addr, rqlen);
			    break;

		    case RCMD_PROGRAMM:
			    RXRom_PGMCmd(rf, value, mem_addr, rqlen);
			    break;

		    case RCMD_BLOCKERASE:
			    RXRom_BLKEraseCmd(rf, value, mem_addr, rqlen);
			    break;

		    case RCMD_READLOCKBIT2:
			    RXRom_LKBitRead2Cmd(rf, value, mem_addr, rqlen);
			    break;

		    case RCMD_PGMLOCKBIT:
			    RXRom_LKBitPGMCmd(rf, value, mem_addr, rqlen);
			    break;

		    default:
			    fprintf(stderr, "Flash: Non first cycle with wrong cmd %02x\n",
				    rf->stateCmd);
			    break;

		}
	}
}

/*
 ***************************************************************************
 * Read an write of data flash when in P/E mode
 ***************************************************************************
 */
static uint32_t
RXFlash_DFlashRead(void *clientData, uint32_t mem_addr, int rqlen)
{
	fprintf(stderr, "IOH mode in %s not implemented\n", __func__);
	return 0;
}

static void
RXFlash_DFlashWrite(void *clientData, uint32_t value, uint32_t mem_addr, int rqlen)
{
	RXFlash *rf = clientData;
	if (rf->fcu_ram_verified == false) {
		if (memcmp(rf->fcu_ram, rf->fcu_rom, FCU_ROM_SIZE) == 0) {
			rf->fcu_ram_verified = true;
		} else {
			/* Maybe a check for a correct user provided content should be added */
			fprintf(stderr, "RX_Flash: Write CMD with wrong FCU RAM content\n");
			exit(1);
			return;
		}
	}
	if (check_busy(rf)) {
		fprintf(stderr, "RX_Flash: DFlashWrite FCU access while busy\n");
		exit(1);
	}
	if (rf->stateCmdCycle == 1) {
		if (rqlen != 1) {
			fprintf(stderr, "RXFlash Write CMD wrong access width %d\n", rqlen);
			return;
		}
		rf->stateRA = mem_addr;
		rf->regFSTATR0 &= ~FSTATR0_FRDY;
		switch (value) {

		    case RCMD_TRANSNORM:
			    break;

		    case RCMD_TRANSSTATUSREAD:
			    break;
#if 0
		    case RCMD_TRANSLOCKREAD:
			    break;
#endif
		    case RCMD_SETPCK:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;

		    case RCMD_PROGRAMM:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;

		    case RCMD_BLOCKERASE:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;

		    case RCMD_PESUSPEND:
			    break;
		    case RCMD_PERESUME:
			    break;
		    case RCMD_CLEARSTATUS:
			    rf->regFSTATR0 = 0;
			    break;
#if 0
		    case RCMD_READLOCKBIT2:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;
		    case RCMD_PGMLOCKBIT:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;
#endif
		    case DCMD_BLANKCHECK:
			    rf->stateCmdCycle++;
			    rf->stateCmd = value;
			    break;

		    default:
			    fprintf(stderr, "Unknown Flash cmd %02x error\n", rf->stateCmd);
			    exit(1);
			    /* Some error */
			    break;
		}
	} else {
		switch (rf->stateCmd) {
		    case RCMD_SETPCK:
			    RXRom_SetPCKCmd(rf, value, mem_addr, rqlen);
			    break;

		    case RCMD_PROGRAMM:
			    RXRom_PGMCmd(rf, value, mem_addr, rqlen);
			    break;

		    case RCMD_BLOCKERASE:
			    RXRom_BLKEraseCmd(rf, value, mem_addr, rqlen);
			    break;

		    case DCMD_BLANKCHECK:
			    RXDFlash_BlankCheckCmd(rf, value, mem_addr, rqlen);
			    break;

		    default:
			    fprintf(stderr, "Flash: Non first cycle with wrong cmd %02x\n",
				    rf->stateCmd);
			    break;

		}
	}
}

static void
Flash_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
	RXFlash *rf = module_owner;
	uint32_t rom_base = base;
	Mem_UnMapRange(base, mapsize);
	Mem_UnMapRange(FCU_RAM_TOP, FCU_RAM_SIZE);
	Mem_UnMapRange(FCU_ROM_TOP, FCU_ROM_SIZE);
	IOH_DeleteRegion(DATA_FLASH_BASE, rf->dfsize);
	IOH_DeleteRegion(rom_base, rf->romsize);
	IOH_DeleteRegion(ROM_BASE_WRITE(rf), rf->romsize);
	Mem_UnMapRange(rom_base, rf->romsize);
	Mem_UnMapRange(DATA_FLASH_BASE, rf->dfsize);
	IOH_Delete8(REG_FMODR(base));
	IOH_Delete8(REG_FASTAT(base));
	IOH_Delete8(REG_FAEINT(base));
	IOH_Delete8(REG_FRDYIE(base));
	IOH_Delete16(REG_DFLRE0(base));
	IOH_Delete16(REG_DFLRE1(base));
	IOH_Delete16(REG_DFLWE0(base));
	IOH_Delete16(REG_DFLWE1(base));
	IOH_Delete16(REG_FCURAME(base));
	IOH_Delete8(REG_FSTATR0(base));
	IOH_Delete8(REG_FSTATR1(base));
	IOH_Delete16(REG_FENTRYR(base));
	IOH_Delete16(REG_FPROTR(base));
	IOH_Delete16(REG_FRESETR(base));
	IOH_Delete16(REG_FCMDR(base));
	IOH_Delete16(REG_FCPSR(base));
	IOH_Delete16(REG_FPESTAT(base));
	IOH_Delete16(REG_DFLBCCNT(base));
	IOH_Delete16(REG_DFLBCSTAT(base));
	IOH_Delete16(REG_PCKAR(base));
	IOH_Delete8(REG_FWEPROR(base));
}

static void
Flash_Map(void *module_owner, uint32_t _base, uint32_t mapsize, uint32_t flags)
{
	RXFlash *rxflash = module_owner;
	uint8_t *rom_mem = rxflash->rom_mem;
	uint32_t rom_base = _base;
	rxflash->rom_base = _base;
	if (rxflash->regFENTRYR & FENTRYR_FENTRY0) {
		IOH_NewRegion(rom_base, rxflash->romsize, RXFlash_RomRead, NULL, HOST_BYTEORDER,
			      rxflash);
		IOH_NewRegion(ROM_BASE_WRITE(rxflash), rxflash->romsize, NULL, RXFlash_RomWrite,
			      HOST_BYTEORDER, rxflash);
		//fprintf(s:derr,"IO mapped the ROM\n");
	} else if (!rxflash->dbgMode) {
		//fprintf(stderr,"Mem mapped the ROM\n");
		Mem_MapRange(rom_base, rom_mem, rxflash->romsize, mapsize,
			     flags & ~MEM_FLAG_WRITABLE);
	} else {
		Mem_MapRange(rom_base, rom_mem, rxflash->romsize, mapsize, flags);
	}
	/*
	 **********************************************************************
	 **********************************************************************
	 */
	if (rxflash->regFENTRYR & FENTRYR_FENTRYD) {
		//fprintf(stderr,"IO mapped the Dataflash\n");
		IOH_NewRegion(DATA_FLASH_BASE, rxflash->dfsize, RXFlash_DFlashRead,
			      RXFlash_DFlashWrite, HOST_BYTEORDER, rxflash);
	} else {
		//fprintf(stderr,"Mem mapped the Dataflash\n");
		Mem_MapRange(DATA_FLASH_BASE, rxflash->data_mem, rxflash->dfsize, rxflash->dfsize,
			     flags);
	}
	if (rxflash->regFCURAME & FCURAME_FCRME) {
		Mem_MapRange(FCU_RAM_TOP, rxflash->fcu_ram, FCU_RAM_SIZE, FCU_RAM_SIZE,
			     MEM_FLAG_WRITABLE | MEM_FLAG_READABLE);
	}
	Mem_MapRange(FCU_ROM_TOP, rxflash->fcu_rom, FCU_ROM_SIZE, FCU_ROM_SIZE,
		     flags & ~MEM_FLAG_WRITABLE);
	IOH_New8(REG_FMODR(base), fmodr_read, fmodr_write, rxflash);
	IOH_New8(REG_FASTAT(base), fastat_read, fastat_write, rxflash);
	IOH_New8(REG_FAEINT(base), faeint_read, faeint_write, rxflash);
	IOH_New8(REG_FRDYIE(base), frdyie_read, frdyie_write, rxflash);
	IOH_New16(REG_DFLRE0(base), dflre0_read, dflre0_write, rxflash);
	IOH_New16(REG_DFLRE1(base), dflre1_read, dflre1_write, rxflash);
	IOH_New16(REG_DFLWE0(base), dflwe0_read, dflwe0_write, rxflash);
	IOH_New16(REG_DFLWE1(base), dflwe1_read, dflwe1_write, rxflash);
	IOH_New16(REG_FCURAME(base), fcurame_read, fcurame_write, rxflash);
	IOH_New8(REG_FSTATR0(base), fstatr0_read, fstatr0_write, rxflash);
	IOH_New8(REG_FSTATR1(base), fstatr1_read, fstatr1_write, rxflash);
	IOH_New16(REG_FENTRYR(base), fentryr_read, fentryr_write, rxflash);
	IOH_New16(REG_FPROTR(base), fprotr_read, fprotr_write, rxflash);
	IOH_New16(REG_FRESETR(base), fresetr_read, fresetr_write, rxflash);
	IOH_New16(REG_FCMDR(base), fcmd_read, fcmd_write, rxflash);
	IOH_New16(REG_FCPSR(base), fcpsr_read, fcpsr_write, rxflash);
	IOH_New16(REG_FPESTAT(base), fpestat_read, fpestat_write, rxflash);
	IOH_New16(REG_DFLBCCNT(base), dflbccnt_read, dlfbccnt_write, rxflash);
	IOH_New16(REG_DFLBCSTAT(base), dflbcstat_read, dflbcstat_write, rxflash);
	IOH_New16(REG_PCKAR(base), pckar_read, pckar_write, rxflash);
	IOH_New8(REG_FWEPROR(base), fwepror_read, fwepror_write, rxflash);
}

static uint32_t
parse_memsize(char *str)
{
	uint32_t size;
	unsigned char c;
	if (sscanf(str, "%d", &size) != 1) {
		return 0;
	}
	if (sscanf(str, "%d%c", &size, &c) == 1) {
		return size;
	}
	switch (tolower(c)) {
	    case 'm':
		    return size * 1024 * 1024;
	    case 'k':
		    return size * 1024;
	}
	return 0;
}

BusDevice *
RXFlash_New(const char *flash_name)
{
	RXFlash *rxflash = sg_new(RXFlash);
	char *imagedir;
	char *romsizestr;
	char *datasizestr;
	uint32_t debugmode = 0;
	imagedir = Config_ReadVar("global", "imagedir");
	romsizestr = Config_ReadVar(flash_name, "romsize");
	datasizestr = Config_ReadVar(flash_name, "datasize");
	if (romsizestr) {
		rxflash->romsize = parse_memsize(romsizestr);
		if (rxflash->romsize == 0) {
			fprintf(stderr, "RX Flash \"%s\" has zero size\n", flash_name);
			return NULL;
		}
	} else {
		fprintf(stderr, "Flash size for CPU is not conigured\n");
		return NULL;
	}
	if (datasizestr) {
		rxflash->dfsize = parse_memsize(datasizestr);
		if (rxflash->dfsize == 0) {
			fprintf(stderr, "RX Flash \"%s\" has zero size\n", flash_name);
			return NULL;
		}
		rxflash->blank_size = rxflash->dfsize >> 6;	/* One blank status bit for every 64 bits of data */
	} else {
		fprintf(stderr, "Flash size for CPU Data Flash is not configured\n");
		return NULL;
	}
	Config_ReadUInt32(&debugmode, flash_name, "debug");
	rxflash->dbgMode = debugmode;
	rxflash->fcu_ram = sg_calloc(FCU_RAM_SIZE);
	if (imagedir) {
		char *mapfile = alloca(strlen(imagedir) + strlen(flash_name) + 20);
		sprintf(mapfile, "%s/%s_rom.img", imagedir, flash_name);
		rxflash->rom_image =
		    DiskImage_Open(mapfile, rxflash->romsize, DI_RDWR | DI_CREAT_FF);
		if (!rxflash->rom_image) {
			fprintf(stderr, "RX-Flash: Open disk image for ROM failed\n");
			exit(1);
		}
		rxflash->rom_mem = DiskImage_Mmap(rxflash->rom_image);

		sprintf(mapfile, "%s/%s_data.img", imagedir, flash_name);
		rxflash->data_image =
		    DiskImage_Open(mapfile, rxflash->dfsize, DI_RDWR | DI_CREAT_00);
		if (!rxflash->data_image) {
			fprintf(stderr, "RX-Flash: Open disk image for Data Flash failed\n");
			exit(1);
		}
		rxflash->data_mem = DiskImage_Mmap(rxflash->data_image);

		sprintf(mapfile, "%s/%s_blank.img", imagedir, flash_name);
		rxflash->blank_image =
		    DiskImage_Open(mapfile, rxflash->blank_size, DI_RDWR | DI_CREAT_FF);
		if (!rxflash->blank_image) {
			fprintf(stderr,
				"RX-Flash: Open disk image for Blank Check Bitmap failed\n");
			exit(1);
		}
		rxflash->blank_mem = DiskImage_Mmap(rxflash->blank_image);

		sprintf(mapfile, "%s/%s_fcu_rom.img", imagedir, flash_name);
		rxflash->fcu_rom_image = DiskImage_Open(mapfile, FCU_ROM_SIZE, DI_RDONLY);
		if (!rxflash->fcu_rom_image) {
			unsigned int i;
			fprintf(stderr, "RX-Flash: Open disk image \"%s\" for FCU ROM failed\n",
				mapfile);
			rxflash->fcu_rom = sg_calloc(FCU_ROM_SIZE);
			for (i = 0; i < FCU_ROM_SIZE; i++) {
				rxflash->fcu_rom[i] = rand();
			}
		} else {
			rxflash->fcu_rom = DiskImage_Mmap(rxflash->fcu_rom_image);
			if (!rxflash->fcu_rom) {
				fprintf(stderr, "MMap of FCU rom failed\n");
				exit(1);
			}
		}
		fprintf(stderr, "Mapped ROM    to %p\n", rxflash->rom_mem);
		fprintf(stderr, "Mapped DFLASH to %p\n", rxflash->data_mem);
		fprintf(stderr, "Mapped BLANK  to %p\n", rxflash->blank_mem);
	} else {
		rxflash->rom_mem = sg_calloc(rxflash->romsize);
		memset(rxflash->rom_mem, 0xff, rxflash->romsize);
		rxflash->data_mem = sg_calloc(rxflash->dfsize);
		memset(rxflash->data_mem, 0x00, rxflash->dfsize);	/* ?? */
		rxflash->blank_mem = sg_calloc(rxflash->blank_size);
		memset(rxflash->blank_mem, 0xff, rxflash->blank_size);
	}
	rxflash->tDP8 = 400;
	rxflash->tDP128 = 1000;
	rxflash->tDE2K = 70000;
	rxflash->tDBC8 = 30;
	rxflash->tDBC2K = 700;
	rxflash->tDSPD = 120;
	rxflash->tDSESD1 = 120;
	rxflash->tDSESD2 = 1700;
	rxflash->tDSEED = 1700;

	rxflash->tP256 = 2400;
	rxflash->tP4K = 27600;
	rxflash->tP16K = 108000;
	rxflash->tE4K = 30000;
	rxflash->tE16K = 120000;
	rxflash->tSPD = 120;
	rxflash->tSESD1 = 120;
	rxflash->tSESD2 = 1700;
	rxflash->tSEED = 1700;

	rxflash->tSETPCKAR = 10;

	rxflash->clkIn = Clock_New("%s.clk", flash_name);
	rxflash->stateCmdCycle = 1;
	rxflash->bdev.first_mapping = NULL;
	rxflash->bdev.Map = Flash_Map;
	rxflash->bdev.UnMap = Flash_UnMap;
	rxflash->bdev.owner = rxflash;
	rxflash->fcu_ram_verified = false;
	if (rxflash->dbgMode) {
		rxflash->bdev.hw_flags = MEM_FLAG_READABLE | MEM_FLAG_WRITABLE;
	} else {
		rxflash->bdev.hw_flags = MEM_FLAG_READABLE;
	}
	fprintf(stderr, "Created RX Flash with size %d bytes\n", rxflash->romsize);
	return &rxflash->bdev;
}
