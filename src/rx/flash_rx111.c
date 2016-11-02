#include <stdint.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <ctype.h>

#include "flash_rx111.h"
#include "configfile.h"
#include "cycletimer.h"
#include "bus.h"
#include "signode.h"
#include "diskimage.h"
#include "sgstring.h"
#include "clock.h"

#define REG_DFLCTL(base)    (0x7FC090)
#define     DFLCTL_DFLEN        (1 << 0)
#define REG_FENTRYR(base)   (0x7FFFB2)
#define     FENTRYR_FENTRY0      (1 << 0)
#define     FENTRYR_FENTRYD      (1 << 7)
#define     FENTRYR_FEKEY_MSK    (0xff << 8)
#define REG_FPR(base)       (0x7FC0C0)
#define REG_FPSR(base)      (0x7FC0C1)
#define     FPSR_PERR           (1 << 0)
#define REG_FPMCR(base)     (0x7FFF80)
#define     FPMCR_FMS0          (1 << 1)
#define     FPMCR_RPDIS         (1 << 3)
#define     FPMCR_FMS1          (1 << 4)
#define     FPMCR_LVPE          (1 << 6)
#define     FPMCR_FMS2          (1 << 7)
#define     FPMCR_FMS_MSK       (0x92)
#define     FMS_MODE_READ       (0)
#define     FMS_MODE_DFLASH_PE  (0x10)
#define     FMS_DISCHARGE1      (0x12)
#define     FMS_MODE_ROM_PE     (0x82)
#define     FMS_DISCHARGE2      (0x92)
#define REG_FISR(base)      (0x7FC0B6)
#define     FISR_PCKA_MSK   (0x1f)
#define     FISR_SAS_MSK    (3 << 6)
#define REG_FRESETR(base)   (0x7FFF89)
#define     FRESETR_FRESET  (1 << 0)
#define REG_FASR(base)      (0x7FFF81)
#define     FASR_EXS        (1 << 0)
#define REG_FCR(base)       (0x7FFF85)
#define     FCR_CMD_MSK     (0xf << 0)
#define     FCR_DRC         (1 << 4)
#define     FCR_STOP        (1 << 6)
#define     FCR_OPST        (1 << 7)
#define REG_FEXCR(base)     (0x7FC0B7)
#define     FEXCR_CMD_MSK   (0xf << 0)
#define     FEXCR_OPST      (1 << 7)

#define REG_FSARH(base)     (0x7FFF84)
#define REG_FSARL(base)     (0x7FFF82)
#define REG_FEARH(base)     (0x7FFF88)
#define REG_FEARL(base)     (0x7FFF86)
#define REG_FRBH(base)      (0x7FC0C4)
#define REG_FRBL(base)      (0x7FC0C2)
#define REG_FWBH(base)      (0x7FFF8E)
#define REG_FWBL(base)      (0x7FFF8C)
#define REG_FSTATR0(base)   (0x7FFF8A)
#define     FSTATR0_ERERR       (1 << 0)
#define     FSTATR0_PRGERR      (1 << 1)
#define     FSTATR0_BCERR       (1 << 3)
#define     FSTATR0_ILGERR      (1 << 4)
#define     FSTATR0_EILGLERR    (1 << 5)
#define REG_FSTATR1(base)   (0x7FFF8B)
#define     FSTATR1_DRRDY       (1 << 1)
#define     FSTATR1_FRDY        (1 << 6)
#define     FSTATR1_EXRDY       (1 << 7)
#define REG_FEAMH(base)     (0x7FC0BA)
#define REG_FEAML(base)     (0x7FC0B8)
#define REG_FSCMR(base)     (0x7FC0B0)
#define     FSCMR_SASMF         (1 << 8)
#define REG_FAWSMR(base)    (0x7FC0B2)
#define REG_FAWEMR(base)    (0x7FC0B4)
#define REG_UIDR(base, x)   (0x850 + (x)) /* Extra area, not normal addr space */

#define DATA_FLASH_BASE 0x00100000

typedef struct RXFlash {
    BusDevice bdev;
    Clock_t *clkIn;
    DiskImage *rom_image;
    DiskImage *data_image;
    uint8_t *rom_mem;
    uint8_t *data_mem;

    bool dbgMode;
    uint32_t dfsize;
    uint32_t rom_base;
    uint32_t romsize;
    uint32_t blank_size;

    uint8_t regDFLCTL;
    uint16_t regFENTRYR;
    uint8_t stateFPR;
    uint8_t regFPSR;
    uint8_t regFPMCR;
    uint8_t regTmpFPMCR;
    uint8_t regFISR;
    uint8_t regFRESETR;
    uint8_t regFASR;
    uint8_t regFCR;
    uint8_t regFEXCR;
    uint32_t regFSAR;
    uint32_t regFEAR;
    uint32_t uidIdx;
    uint32_t regFRB;
    uint32_t regFWB;
    uint8_t regFSTATR0;
    uint8_t regFSTATR1;
    uint32_t regFEAM;
    uint16_t regFSCMR;
    uint16_t regFAWSMR;
    uint16_t regFAWEMR;
//UIDR
    uint8_t uuid[32];
    uint32_t tDP1;
    uint32_t tDE1K;
    uint32_t tDBC1;
    uint32_t tDBC1K;
    uint32_t tDSED;
    uint32_t tDSTOP;

    uint32_t tP4;
    uint32_t tE1K;
    uint32_t tBC4;
    uint32_t tBC1K;
    uint32_t tSED;
    uint32_t tSAS;
    uint32_t tAWS;
    uint32_t tDIS;
    uint32_t tMS;
    CycleCounter_t busy_until;
} RXFlash;

/**
 ***********************************************************************
 * \fn static void make_busy(RXFlash * rf, uint32_t useconds)
 ***********************************************************************
 */
static void
make_busy(RXFlash * rf, uint32_t useconds)
{
    rf->busy_until = CycleCounter_Get() + MicrosecondsToCycles(useconds);
    rf->regFSTATR1 &= ~FSTATR1_FRDY;
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


/**
 ********************************************************************
 * \fn static uint32_t parse_memsize(char *str)
 * Parse the memory size string from the configuration file.
 ********************************************************************
 */
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

/**
 **************************************************************************************
 * Implementation of the "Flash Write" Operation 
 **************************************************************************************
 */
static void
RXFlash_Programm(RXFlash *rf)
{
    switch(rf->regFPMCR &  FPMCR_FMS_MSK) {
        case FMS_MODE_DFLASH_PE:
            {
                uint32_t addr = rf->regFSAR - 0xF1000;
                uint8_t value = rf->regFWB;
                if (addr >= rf->dfsize) {
                   fprintf(stderr, "Illegal data flash address\n"); 
                   break;
                } else if(rf->data_mem[addr] != 0xff) {
                    fprintf(stderr, "Blank check error at 0x%04x\n", addr); 
                    break;
                } else {
                    rf->data_mem[addr] = value;
                    make_busy(rf, rf->tDP1);
                }
            }
            break;

        case FMS_DISCHARGE1: 
            break;

        case FMS_MODE_ROM_PE:
            {
                uint32_t addr = rf->regFSAR;
                uint32_t arrIdx = (addr - (512 * 1024) + rf->romsize) & ~3;
                uint32_t fwb = rf->regFWB;
                uint8_t *data;
                if (arrIdx >= rf->romsize) {
                    fprintf(stderr, "Access outside of flash\n");
                } else {
                    //fprintf(stderr, "Write %08x to %08x\n", fwb, arrIdx);
                    data = rf->rom_mem + arrIdx;
                    data[0] = fwb & 0xff; 
                    data[1] = (fwb >> 8) & 0xff;
                    data[2] = (fwb >> 16) & 0xff;
                    data[3] = (fwb >> 24) & 0xff;
                }
            }
            break;

        case FMS_DISCHARGE2:
            break;
        default:
            fprintf(stderr, "Illegal fpmcr mode 0x%02x\n", rf->regFPMCR);
            break;
    }
}

static void
RXFlash_BlockErase(RXFlash *rf)
{
    switch(rf->regFPMCR &  FPMCR_FMS_MSK) {
        case FMS_MODE_DFLASH_PE:
            {
                uint32_t saddr = rf->regFSAR - 0xF1000;
                uint32_t eaddr = rf->regFEAR - 0xF1000;
                if ((saddr >= rf->dfsize) || (eaddr >= rf->dfsize)) {
                   fprintf(stderr, "Illegal data flash address\n"); 
                   break;
                } if (((saddr & 0x3ff) != 0) || ((eaddr & 0x3ff) != 0x3ff)) {
                   fprintf(stderr, "Illegal block address for dflash\n"); 
                   break;
                } else {
                    bool blank = true;
                    uint32_t i;
                    for (i = saddr; i <= eaddr; i++) {
                        if (rf->data_mem[i] != 0xff) {
                            blank = false;
                            break;
                        } 
                    }
                    memset(rf->data_mem + saddr, 0xff, eaddr - saddr + 1);
                    if (blank) {
                        make_busy(rf, rf->tDE1K / 11); /* Experimentaly determined value */
                    } else {
                        make_busy(rf, rf->tDE1K);
                    }
                }
                
            }
            break;

        case FMS_DISCHARGE1: 
            break;

        case FMS_MODE_ROM_PE:
            {
                uint32_t addr = rf->regFSAR;
                uint32_t eaddr = rf->regFEAR;
                uint8_t *data;
                uint32_t arrIdx = (addr - (512 * 1024) + rf->romsize);
                if ((addr + 0x3ff) != eaddr) {
                    fprintf(stderr, "Erase: Bad end addr\n");
                    rf->regFSTATR0 |= FSTATR0_ILGERR; 
                } else if (arrIdx  & 0x3ff) {
                    fprintf(stderr, "Erase: Bad block alignment\n");
                    rf->regFSTATR0 |= FSTATR0_ILGERR; 
                } else if (arrIdx >= rf->romsize) {
                    fprintf(stderr, "Access outside of flash\n");
                    rf->regFSTATR0 |= FSTATR0_ILGERR; 
                } else {
                    data = rf->rom_mem + arrIdx;
                    memset(data, 0xff, 1024);
                    make_busy(rf, rf->tE1K);
                }
            }
            break;
        case FMS_DISCHARGE2:
            break;
        default:
            fprintf(stderr, "Illegal fpmcr mode 0x%02x\n", rf->regFPMCR);
            break;
    }
}

static void
RXFlash_UniqueIdRead(RXFlash *rf, uint8_t cmd)
{
    //fprintf(stderr, "Cmd %02x\n", cmd);
    if((rf->regFSAR < 0x850) || (rf->regFSAR > 0x86f)) {
            rf->regFSTATR0 |= FSTATR0_ILGERR; 
            return;
    }
    if((rf->regFEAR < 0x850) || (rf->regFEAR > 0x86f)) {
            rf->regFSTATR0 |= FSTATR0_ILGERR; 
            return;
    }
    if (cmd & FCR_DRC) {
        rf->uidIdx++;
        rf->regFSTATR1 &= ~FSTATR1_DRRDY;
    } else {
        if(((rf->uidIdx << 2) + rf->regFSAR) > rf->regFEAR) {
            make_busy(rf, 1);
            //fprintf(stderr, "UID address out of range\n");   
        } else if(((rf->uidIdx << 2) + rf->regFSAR) < rf->regFSAR) {
            fprintf(stderr, "UID address out of range\n");   
            rf->regFSTATR0 |= FSTATR0_ILGERR; 
        } else {
            unsigned int i = (rf->uidIdx << 2) & 31;
            rf->regFRB = rf->uuid[i] | (((uint32_t)rf->uuid[i + 1]) << 8) | 
                        (((uint32_t)rf->uuid[i + 2]) << 16) | (((uint32_t)rf->uuid[i + 3]) << 24);
            rf->regFSTATR1 |= FSTATR1_DRRDY;
        }
    }

}
/**
 ************************************************************************************
 * \fn static void RXFlash_BlankCheck(RXFlash *rf)
 * Does blank check in byte resoultion. 
 ************************************************************************************
 */
static void
RXFlash_BlankCheck(RXFlash *rf)
{
    switch(rf->regFPMCR &  FPMCR_FMS_MSK) {
        case FMS_MODE_DFLASH_PE:
            {
                uint32_t saddr = rf->regFSAR - 0xF1000;
                uint32_t eaddr = rf->regFEAR - 0xF1000;
                if ((saddr >= rf->dfsize) || (eaddr >= rf->dfsize)) {
                   fprintf(stderr, "Illegal data flash address\n"); 
                   break;
                } else {
                    uint32_t i;
                    for (i = saddr; i <= eaddr; i++) {
                        if (rf->data_mem[i] != 0xff) {
                            rf->regFSTATR0 |= FSTATR0_BCERR;
                            break;
                        } 
                    }
                    if (eaddr != saddr) {
                        uint32_t time = rf->tDBC1 + (((eaddr - saddr - 1) * (rf->tBC1K - rf->tDBC1)) / 1024);
                        make_busy(rf, time);
                    }
                }
            }
            break;

        case FMS_DISCHARGE1: 
            break;
        case FMS_MODE_ROM_PE:
            {
                uint32_t addr = rf->regFSAR;
                uint32_t eaddr = rf->regFEAR;
                uint32_t cnt = eaddr - addr;
                uint8_t *data;
                uint32_t arrIdx = (addr - (512 * 1024) + rf->romsize);
                int i;
                if ((arrIdx + cnt) > rf->romsize) {
                    fprintf(stderr, "Blankchk: End is past flash\n");
                    rf->regFSTATR0 |= FSTATR0_ILGERR; 
                } else if (arrIdx >= rf->romsize) {
                    fprintf(stderr, "Access outside of flash\n");
                    rf->regFSTATR0 |= FSTATR0_ILGERR; 
                } else {
                    data = rf->rom_mem + arrIdx;
                    for (i = 0; i < cnt; i++) {
                        if(data[i] != 0xff) {
                            rf->regFSTATR0 |= FSTATR0_BCERR;
                            break;
                        }
                    }
                }
            }
            break;

        case FMS_DISCHARGE2:
            break;
        default:
            fprintf(stderr, "Illegal fpmcr mode 0x%02x\n", rf->regFPMCR);
            break;
    }

}
/**
 *********************************************************************
 * Do a command. The action is determined by FCR register and
 * current mode of the flash controller.
 *********************************************************************
 */
static void
RXFlash_StartOperation(RXFlash *rf, uint8_t fcr) 
{
    
    uint8_t cmd = fcr & FCR_CMD_MSK;
    rf->regFSTATR0 = 0;
    switch(cmd) {
        case 1: /* Programm */
            RXFlash_Programm(rf);
            break;
        case 3: /* Blank check */
            RXFlash_BlankCheck(rf);
            break;
        case 4: /* Block erase */
            RXFlash_BlockErase(rf);
            break;
        case 5: /* Unique ID read */
            RXFlash_UniqueIdRead(rf, fcr);
            break;
        default:
            fprintf(stderr, "Nonexisting command to flash controller\n");
            rf->regFSTATR0 |= FSTATR0_ILGERR; 
            break;
    }
}

static uint32_t
dflctl_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regDFLCTL;
}

static void
dflctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    uint8_t diff = rf->regDFLCTL; 
    rf->regDFLCTL = value & DFLCTL_DFLEN;
    if (diff & DFLCTL_DFLEN) {
        /* Make it busy for 5us somehow is missing here */
        Mem_AreaUpdateMappings(&rf->bdev);
    }
}

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
        rf->regFENTRYR = value & 0x0081; 
        if(diff & 0x81) {
            Mem_AreaUpdateMappings(&rf->bdev);
        }
    } else {
        fprintf(stderr, "Wrong FENTRYR key, write 0x%04x\n", value);
    }
}

static uint32_t
fpr_read(void *clientData, uint32_t address, int rqlen)
{
    fprintf(stderr, "FPR is a writeonly register\n");
    return 0;
}

static void
fpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    if ((value != 0xa5)) {
        /* sequenzfehler */
        rf->regTmpFPMCR = 0xff;
        rf->regFPSR |=FPSR_PERR;
        fprintf(stderr, "FPC: Only write of 0xa5 is allowed but detected 0x%02x\n", value);
    } else {
        rf->stateFPR = 1;
    }
}

static uint32_t
fpsr_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
fpsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

static uint32_t
fpmcr_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFPMCR;
}

/**
 ************************************************************************************
 * Bit 1: FMS0     
 * Bit 3: RPDIS   
 * Bit 4: FMS1   
 * Bit 6: LVPE  
 * Bit 7: FMS2 
 ************************************************************************************
 */
static void
fpmcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    if(rf->stateFPR == 1) {
        rf->regTmpFPMCR = value;
        rf->stateFPR = 2;     
    } else if(rf->stateFPR == 2) {
        if ((uint8_t)~value == rf->regTmpFPMCR) {
            rf->stateFPR = 3;     
        } else {
            rf->stateFPR = 0;
            rf->regFPSR |= FPSR_PERR;
            fprintf(stderr, "FPMCR second value not inverse of first: %02x, %02x\n", rf->regTmpFPMCR, value);
        }
    } else  if(rf->stateFPR == 3) {
        if (value == rf->regTmpFPMCR) {
            // success, do action 
            if (check_busy(rf)) {
                fprintf(stderr, "Switching mode of flash while busy\n");
            } else {
                rf->regFPMCR = value & 0xda;
                rf->stateFPR = 0;
                rf->uidIdx = 0;
            }
        } else {
            rf->stateFPR = 0;
            rf->regFPSR |= FPSR_PERR;
            fprintf(stderr, "FPMCR sequence error third value not same as first\n");
        }
    } else {
        /* Sequenzfehler */
        rf->stateFPR = 0;
        rf->regFPSR |= FPSR_PERR;
        fprintf(stderr, "FPMCR sequence error in %s, state %u\n", __func__, rf->stateFPR);
    }
}

static uint32_t
fisr_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFISR;
}

static void
fisr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFISR = value & 0xdf; 
    // Update Startuparea is missing here 
}

/**
 *************************************************************
 *************************************************************
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
    uint8_t diff = rf->regFRESETR ^ value;
    rf->regFRESETR = value & FRESETR_FRESET;
    if((value & FRESETR_FRESET) || (diff & FRESETR_FRESET)) {
        /* Do reset */
        rf->regFASR = 0;
        rf->regFSAR = 0;
        rf->regFEAR = 0;
        rf->regFWB = 0;
        rf->regFCR = 0;
        rf->regFEXCR = 0;
    }
}

static uint32_t
fasr_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFASR;
}

static void
fasr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFASR = value & 1;
}

/**
 *********************************************************************************
 * Flash control register
 *********************************************************************************
 */
static uint32_t
fcr_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFCR;
}

static void
fcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFCR = value & 0xdf;
    if ((value & (FCR_OPST | FCR_STOP)) == FCR_OPST) {
        //fprintf(stderr, "FCR write 0x%08x\n", value);
        RXFlash_StartOperation(rf, rf->regFCR);
    } else if (value == 0) {
        if (check_busy(rf)) {
            fprintf(stderr, "Setting FCR_CLEAR while busy\n");
        } else {
            rf->regFSTATR1 &= ~FSTATR1_FRDY;
        }   
    }
}

static uint32_t
fexcr_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFEXCR;
}

static void
fexcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFEXCR = value & 0x87; 
    if (value & FEXCR_OPST) {
        /* Do something */
    }
}

static uint32_t
fsarh_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFSAR >> 16;
}

static void
fsarh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFSAR = (rf->regFSAR & 0xffff) | ((value & 0xf) << 16);
}

static uint32_t
fsarl_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFSAR & 0xffff;
}

static void
fsarl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFSAR = (rf->regFSAR & 0xffff0000) | value;
}

static uint32_t
fearh_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFEAR >> 16;
}

static void
fearh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFEAR = (rf->regFEAR & 0xffff) | ((value & 0xf) << 16);
}

static uint32_t
fearl_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFEAR & 0xffff;
}

static void
fearl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFEAR = (rf->regFEAR & 0xffff0000) | value;
}

static uint32_t
frbh_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFRB >> 16;
}

static void
frbh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFRB = (rf->regFRB & 0xffff) | (value << 16);
}

static uint32_t
frbl_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFRB & 0xffff;
}

static void
frbl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFRB = (rf->regFRB & 0xffff0000) | value;
}

static uint32_t
fwbh_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFWB >> 16;
}

static void
fwbh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFWB = (rf->regFWB & 0xffff) | (value << 16);
}

static uint32_t
fwbl_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFWB & 0xffff;
}

static void
fwbl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFWB = (rf->regFWB & 0xffff0000) | value;
}

static uint32_t
fstatr0_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFSTATR0;
}

static void
fstatr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "FSTATR0 is not writable\n");
}

static uint32_t
fstatr1_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    if (check_busy(rf)) {
        rf->regFSTATR1 &= ~FSTATR1_FRDY;
    } else if(rf->regFCR == 0) {
        rf->regFSTATR1 &= ~FSTATR1_FRDY;
    } else {
        rf->regFSTATR1 |= FSTATR1_FRDY;
    }
    //fprintf(stderr, "Read fstatr1 %08x\n",rf->regFSTATR1);
    return rf->regFSTATR1;
}

static void
fstatr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "FSTATR1 is not writable\n");
}

static uint32_t
feamh_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFEAM >> 16;         
}

static void
feamh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "FEAMH is not writable\n");
}

static uint32_t
feaml_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFEAM & 0xffff;         
}

static void
feaml_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    fprintf(stderr, "FEAML is not writable\n");
}

static uint32_t
fscmr_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    uint16_t value = rf->regFSCMR;
    value = (value | 0x7e00) & ~(0x80ff);
    return value;
}

static void
fscmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    rf->regFSCMR = value & FSCMR_SASMF;
}

static uint32_t
fawsmr_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFAWSMR;
}

static void
fawsmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    // 
}

static uint32_t
fawemr_read(void *clientData, uint32_t address, int rqlen)
{
    RXFlash *rf = clientData;
    return rf->regFAWEMR;
}

static void
fawemr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}

#if 0
static uint32_t
uidr_read(void *clientData, uint32_t address, int rqlen)
{
}

static void
uidr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
}
#endif

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

}
/*
 ***************************************************************************
 * Read an write of data flash when in P/E mode
 ***************************************************************************
 */
static uint32_t
RXFlash_RomRead(void *clientData, uint32_t mem_addr, int rqlen)
{
    //RXFlash *rf = clientData;
    fprintf(stderr, "IOH mode in %s not implemented\n", __func__);
//    rf->regFASTAT |= FASTAT_CMDLK;
//    rf->regFSTATR0 |= FASTAT_ROMAE;
    return 0;
}


static void
RXFlash_RomWrite(void *clientData, uint32_t value, uint32_t mem_addr, int rqlen)
{
    //  RXFlash *rf = clientData;
}

static void
Flash_UnMap(void *module_owner, uint32_t base, uint32_t mapsize)
{
    RXFlash *rf = module_owner;
    uint32_t rom_start = 0 - rf->romsize;
    uint32_t romsize = rf->romsize;
    IOH_DeleteRegion(DATA_FLASH_BASE, rf->dfsize);
    IOH_DeleteRegion(rom_start, romsize);
    Mem_UnMapRange(DATA_FLASH_BASE, rf->dfsize);
    Mem_UnMapRange(rom_start, romsize);
        //fprintf(stderr, "Munmapping the rom to 0x%08x\n", rom_start);

    IOH_Delete8(REG_DFLCTL(base));
    IOH_Delete16(REG_FENTRYR(base));
    IOH_Delete8(REG_FPR(base));
    IOH_Delete8(REG_FPSR(base));
    IOH_Delete8(REG_FPMCR(base));
    IOH_Delete8(REG_FISR(base));
    IOH_Delete8(REG_FRESETR(base));
    IOH_Delete8(REG_FASR(base));
    IOH_Delete8(REG_FCR(base));
    IOH_Delete8(REG_FEXCR(base));
    IOH_Delete8(REG_FSARH(base));
    IOH_Delete16(REG_FSARL(base));
    IOH_Delete8(REG_FEARH(base));
    IOH_Delete16(REG_FEARL(base));
    IOH_Delete16(REG_FRBH(base));
    IOH_Delete16(REG_FRBL(base));
    IOH_Delete16(REG_FWBH(base));
    IOH_Delete16(REG_FWBL(base));
    IOH_Delete8(REG_FSTATR0(base));
    IOH_Delete8(REG_FSTATR1(base));
    IOH_Delete8(REG_FEAMH(base));
    IOH_Delete16(REG_FEAML(base));
    IOH_Delete16(REG_FSCMR(base));
    IOH_Delete16(REG_FAWSMR(base));
    IOH_Delete16(REG_FAWEMR(base));
}

static void
Flash_Map(void *module_owner, uint32_t _base, uint32_t _mapsize, uint32_t flags)
{
    RXFlash *rxflash = module_owner;
    uint32_t rom_start;
    uint32_t romsize = rxflash->romsize;
    uint8_t *rom_mmap;
    rom_mmap = rxflash->rom_mem;
    //fprintf(stderr, "Mapping rom %08lx\n", rom_mmap);

    rom_start = 0 - rxflash->romsize;
    if (rxflash->regFENTRYR & FENTRYR_FENTRY0) {
        //fprintf(stderr, "FENTRYR0 is set (ROM can be placed in P/E mode)\n" );
        IOH_NewRegion(rom_start, romsize, RXFlash_RomRead, NULL, HOST_BYTEORDER,
            rxflash);
#if 0
        IOH_NewRegion(rom_start & 0x00ffffff, rom_size, NULL, RXFlash_RomWrite,
            HOST_BYTEORDER, rxflash);
#endif
    } else if (!rxflash->dbgMode) {
        Mem_MapRange(rom_start, rom_mmap, romsize, romsize, flags & ~ MEM_FLAG_WRITABLE);
    //    fprintf(stderr, "Memmapping the rom to 0x%08x, size %u\n", rom_start, romsize);
    } else {
     //   fprintf(stderr, "Memmapping the rom to 0x%08x\n", rom_start);
        Mem_MapRange(rom_start, rom_mmap, romsize, romsize, flags);
    }

    if (rxflash->regFENTRYR & FENTRYR_FENTRYD) {
        if (rxflash->regDFLCTL & DFLCTL_DFLEN) {
            IOH_NewRegion(DATA_FLASH_BASE, rxflash->dfsize, RXFlash_DFlashRead,
                  RXFlash_DFlashWrite, HOST_BYTEORDER, rxflash);
        } else {
            fprintf(stderr, "Error: Using disabled flash\n");
            exit(1);
        }
    } else {
       // fprintf(stderr, "Memmapping the data flash to 0x%08x\n", DATA_FLASH_BASE);
        Mem_MapRange(DATA_FLASH_BASE, rxflash->data_mem, rxflash->dfsize, rxflash->dfsize,
                 flags & ~MEM_FLAG_WRITABLE);
    }
    IOH_New8(REG_DFLCTL(base), dflctl_read, dflctl_write, rxflash);
    IOH_New16(REG_FENTRYR(base), fentryr_read, fentryr_write, rxflash);
    IOH_New8(REG_FPR(base), fpr_read, fpr_write, rxflash);
    IOH_New8(REG_FPSR(base), fpsr_read, fpsr_write, rxflash);
    IOH_New8(REG_FPMCR(base), fpmcr_read, fpmcr_write, rxflash);
    IOH_New8(REG_FISR(base), fisr_read, fisr_write, rxflash);
    IOH_New8(REG_FRESETR(base), fresetr_read, fresetr_write, rxflash);
    IOH_New8(REG_FASR(base), fasr_read, fasr_write, rxflash);
    IOH_New8(REG_FCR(base), fcr_read, fcr_write, rxflash);
    IOH_New8(REG_FEXCR(base), fexcr_read, fexcr_write, rxflash);
    IOH_New8(REG_FSARH(base), fsarh_read, fsarh_write, rxflash);
    IOH_New16(REG_FSARL(base), fsarl_read, fsarl_write, rxflash);
    IOH_New8(REG_FEARH(base), fearh_read, fearh_write, rxflash);
    IOH_New16(REG_FEARL(base), fearl_read, fearl_write, rxflash);
    IOH_New16(REG_FRBH(base), frbh_read, frbh_write, rxflash);
    IOH_New16(REG_FRBL(base), frbl_read, frbl_write, rxflash);
    IOH_New16(REG_FWBH(base), fwbh_read, fwbh_write, rxflash);
    IOH_New16(REG_FWBL(base), fwbl_read, fwbl_write, rxflash);
    IOH_New8(REG_FSTATR0(base), fstatr0_read, fstatr0_write, rxflash);
    IOH_New8(REG_FSTATR1(base), fstatr1_read, fstatr1_write, rxflash);
    IOH_New8(REG_FEAMH(base), feamh_read, feamh_write, rxflash);
    IOH_New16(REG_FEAML(base), feaml_read, feaml_write, rxflash);
    IOH_New16(REG_FSCMR(base), fscmr_read, fscmr_write, rxflash);
    IOH_New16(REG_FAWSMR(base), fawsmr_read, fawsmr_write, rxflash);
    IOH_New16(REG_FAWEMR(base), fawemr_read, fawemr_write, rxflash);
}

BusDevice *
RX111Flash_New(const char *flash_name)
{
    RXFlash *rxflash = sg_new(RXFlash);
    char *imagedir;
    char *romsizestr;
    char *datasizestr;
    int i;
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
        rxflash->blank_size = rxflash->dfsize >> 4; /* One blank status bit for every two Bytes of data */
    } else {
        fprintf(stderr, "Flash size for CPU Data Flash is not configured\n");
        return NULL;
    }
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
        DiskImage_Open(mapfile, rxflash->dfsize, DI_RDWR | DI_CREAT_FF);
        if (!rxflash->data_image) {
            fprintf(stderr, "RX-Flash: Open disk image for Data Flash failed\n");
            exit(1);
        }
        rxflash->data_mem = DiskImage_Mmap(rxflash->data_image);

        fprintf(stderr, "Mapped ROM    to %p\n", rxflash->rom_mem);
        fprintf(stderr, "Mapped DFLASH to %p\n", rxflash->data_mem);
    } else {
        rxflash->rom_mem = sg_calloc(rxflash->romsize);
        memset(rxflash->rom_mem, 0xff, rxflash->romsize);
        rxflash->data_mem = sg_calloc(rxflash->dfsize);
        memset(rxflash->data_mem, 0x00, rxflash->dfsize);    /* ?? */
    }

    rxflash->regTmpFPMCR = 0xff;
    rxflash->regDFLCTL = 0; 
    rxflash->regFENTRYR = 0; 
    //rxflash->regFPR = random();
    rxflash->regFPSR = 0;
    rxflash->regFPMCR = 0x08;
    rxflash->regFISR = 0; 
    rxflash->regFRESETR = 0; 
    rxflash->regFASR = 0; 
    rxflash->regFCR = 0;
    rxflash->regFEXCR = 0; 
    rxflash->regFSAR = 0; 
    rxflash->regFEAR = 0; 
    rxflash->regFRB = 0; 
    rxflash->regFWB = 0; 
    rxflash->regFSTATR0 = 0; 
    rxflash->regFSTATR1 = 0x04; 
    rxflash->regFEAM = 0; 
    rxflash->regFSCMR = 0xff00;
    rxflash->regFAWSMR = 0x03ff;
    rxflash->regFAWEMR = 0x03ff;

    rxflash->dbgMode = false;
    rxflash->clkIn = Clock_New("%s.clk", flash_name);
    rxflash->bdev.first_mapping = NULL;
    rxflash->bdev.Map = Flash_Map;
    rxflash->bdev.UnMap = Flash_UnMap;
    rxflash->bdev.owner = rxflash;
    if (rxflash->dbgMode) {
        rxflash->bdev.hw_flags = MEM_FLAG_READABLE | MEM_FLAG_WRITABLE;
    } else {
        rxflash->bdev.hw_flags = MEM_FLAG_READABLE;
    }

    /* For 32MHz */
    rxflash->tDP1 = 41;
    rxflash->tDE1K = 6150;
    rxflash->tDBC1 = 16;
    rxflash->tDBC1K = 127;
    rxflash->tDSED = 13;
    rxflash->tDSTOP = 5;

    rxflash->tP4 = 52;
    rxflash->tE1K = 5480;
    //rxflash->tE1K = 170000; 
    rxflash->tBC4 = 16;
    rxflash->tBC1K = 127;
    rxflash->tSED = 13;
    rxflash->tSAS = 6160;
    rxflash->tAWS = 6160;
    rxflash->tDIS = 2;
    rxflash->tMS = 5;
    for(i = 0; i < 32; i++) {
        rxflash->uuid[i] = i;
    }
    return &rxflash->bdev;
}
