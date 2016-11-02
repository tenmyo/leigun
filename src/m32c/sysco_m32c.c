#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "bus.h"
#include "sgstring.h"
#include "clock.h"
#include "sysco_m32c.h"
#include "signode.h"
#include "configfile.h"
#include "cycletimer.h"

#define SYS_PM1		(0x05)
#define CG_CM0		(0x06)
#define		CM0_7	(1 << 7)
#define CG_CM1		(0x07)
#define 	CM1_7	(1 << 7)
#define CG_MCD		(0x0c)
#define CG_CM2		(0x0d)
#define		CM2_1	(1 << 1)
#define CG_PLC0		(0x26)
#define CG_PLC1 	(0x27)
#define CG_PM2		(0x13)
#define		PM2_5	(1 << 5)
#define CG_TCSPR	(0x35f)

#define REG_WDTS    (0x0e)
#define REG_WDC     (0xf)
#define     WDC_WDC7    (1 << 7)
#define     WDC_WDC5    (1 << 5)


typedef struct M32C_Sysco {
	BusDevice bdev;
	uint8_t reg_pm1;
	uint8_t reg_cm0;
	uint8_t reg_cm1;
	uint8_t reg_mcd;
	uint8_t reg_cm2;
	uint8_t reg_plc0;
	uint8_t reg_plc1;
	uint8_t reg_pm2;
	uint8_t reg_tcspr;
    uint8_t reg_wdc;
	Clock_t *clk_xin;
	Clock_t *clk_xcin;
	Clock_t *clk_froc;
	Clock_t *clk_fc;
	Clock_t *clk_fcan;
	Clock_t *clk_can;
	Clock_t *clk_fpfc;
	Clock_t *clk_main;
	Clock_t *clk_fpll;
	/* Outputs of CM17,CM21 and MCD */
	Clock_t *clk_cm17;
	Clock_t *clk_cm21;
	Clock_t *clk_mcd;
	Clock_t *clk_cm07;
	Clock_t *clk_pm24;
	Clock_t *clk_fcpu;

	Clock_t *clk_fad;
	Clock_t *clk_f1;
	Clock_t *clk_f8;
	Clock_t *clk_f32;
	Clock_t *clk_fc32;
	Clock_t *clk_f2n;
	Clock_t *clk_wdtPre;
	Clock_t *clk_wdt;

    CycleTimer wdtTimer;
    CycleCounter_t wdtAccCycles;
    CycleCounter_t wdtLastActualized;
    uint32_t wdtCounter;

	SigNode *sigPRC0;
} M32C_Sysco;

static void
wdtActualizeCounter(M32C_Sysco *cgen)
{
    CycleCounter_t cycles;
    CycleCounter_t now;
    FractionU64_t frac;
    uint64_t wdt_cycles;
    uint64_t div;

    now = CycleCounter_Get();
    cycles = now - cgen->wdtLastActualized;
    cgen->wdtLastActualized = now;
    cgen->wdtAccCycles += cycles;
    frac = Clock_MasterRatio(cgen->clk_wdt);
    if (!frac.nom || !frac.denom) {
        fprintf(stderr, "Watchdog: Clock not running\n");
        return;
    }
    div = frac.denom / frac.nom;
    if (div == 0) {
        div = 1;
    }
    wdt_cycles = cgen->wdtAccCycles / div;
    cgen->wdtAccCycles = cgen->wdtAccCycles - wdt_cycles * div;
    if (wdt_cycles >= cgen->wdtCounter) {
        if (cgen->wdtCounter != 0) {
            // Trigger the event here
            cgen->wdtCounter = 0;
        }
    } else {
        cgen->wdtCounter -= wdt_cycles;
    }
    //dbgprintf("Updated watchdog counter to %u\n",wdt->wdtCounter);
}

static void
wdtUpdateTimeout(M32C_Sysco *cgen)
{
    uint64_t wdt_cycles;
    CycleCounter_t cycles;
    FractionU64_t frac;
    frac = Clock_MasterRatio(cgen->clk_wdt);
    if (!frac.nom || !frac.denom) {
        fprintf(stderr, "Watchdog: clock not running\n");
        return;
    }
    if (cgen->wdtCounter == 0) {
        return;
    }
    wdt_cycles = cgen->wdtCounter;
    cycles = wdt_cycles * (frac.denom / frac.nom);
    cycles -= cgen->wdtAccCycles;
    //dbgprintf("Watchdog new timeout in %llu, cycles %llu\n",wdt_cycles,cycles);
    CycleTimer_Mod(&cgen->wdtTimer, cycles);
}

static void
wdt_timeout(void *eventData)
{
	M32C_Sysco *cgen = eventData;
    wdtActualizeCounter(cgen);
    fflush(stderr);
    fflush(stdout);
    fprintf(stderr, "\nWatchdog timeout, Reset\n");
    fflush(stderr);
    exit(0);
}

static uint32_t
pm1_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	return cgen->reg_pm1;
}

static void
pm1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	cgen->reg_pm1 = value;
}

static void
update_cm07(M32C_Sysco * cgen)
{
	if (cgen->reg_cm0 & CM0_7) {
		Clock_MakeDerived(cgen->clk_cm07, cgen->clk_fc, 1, 1);
	} else {
		Clock_MakeDerived(cgen->clk_cm07, cgen->clk_mcd, 1, 1);
	}
}

static uint32_t
cm0_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	return cgen->reg_cm0;
}

static void
cm0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	// check prc0 is missing
	M32C_Sysco *cgen = clientData;
	uint8_t diff = cgen->reg_cm0 ^ value;
	if (SigNode_Val(cgen->sigPRC0) == SIG_LOW) {
		fprintf(stderr, "%s: write protected by PRC0\n", __func__);
		return;
	}
	cgen->reg_cm0 = value;
	if (diff & CM0_7) {
		update_cm07(cgen);
	}
}

static void
update_cm17(M32C_Sysco * cgen)
{
	if (cgen->reg_cm1 & CM1_7) {
		Clock_MakeDerived(cgen->clk_cm17, cgen->clk_fpll, 1, 1);
		//fprintf(stderr,"Connect CM17 with fpll\n");
		//sleep(3);
	} else {
		Clock_MakeDerived(cgen->clk_cm17, cgen->clk_main, 1, 1);
		//fprintf(stderr,"Connect CM17 with main\n");
		//sleep(3);
	}
}

static uint32_t
cm1_read(void *clientData, uint32_t address, int rqlen)
{

	M32C_Sysco *cgen = clientData;
	return cgen->reg_cm1;
}

static void
cm1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	uint8_t diff = cgen->reg_cm1 ^ value;
	if (SigNode_Val(cgen->sigPRC0) == SIG_LOW) {
		fprintf(stderr, "%s: write protected by PRC0\n", __func__);
		return;
	}
	cgen->reg_cm1 = value;
	//fprintf(stderr,"CM1 write %02x, diff %08x\n",value,diff);
	//sleep(3);
	if (diff & CM1_7) {
		update_cm17(cgen);
	}
}

static void
update_mcd(M32C_Sysco * cgen)
{
	int divider;
	switch (cgen->reg_mcd & 0x1f) {
	    case 0x12:
		    divider = 1;
		    break;
	    case 0:
		    divider = 16;
		    break;
	    case 2:
	    case 3:
	    case 4:
	    case 6:
	    case 8:
	    case 10:
	    case 12:
	    case 14:
		    divider = cgen->reg_mcd & 0x1f;
		    break;
	    default:
		    fprintf(stderr, "CGEN: Illegal MCD divider 0x%02x\n", cgen->reg_mcd);
		    return;
	}
	Clock_MakeDerived(cgen->clk_mcd, cgen->clk_cm21, 1, divider);
}

static uint32_t
mcd_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	return cgen->reg_mcd;
}

#include "cpu_m32c.h"
static void
mcd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	uint8_t diff = cgen->reg_mcd ^ value;
	if (SigNode_Val(cgen->sigPRC0) == SIG_LOW) {
		fprintf(stderr, "%s: write protected by PRC0 at 0x%06x\n", __func__,M32C_REG_PC);
		return;
	}
	cgen->reg_mcd = value;
	if (diff) {
		update_mcd(cgen);
	}
}

static void
update_cm21(M32C_Sysco * cgen)
{
	if (cgen->reg_cm2 & CM2_1) {
		Clock_MakeDerived(cgen->clk_cm21, cgen->clk_froc, 1, 1);
	} else {
		Clock_MakeDerived(cgen->clk_cm21, cgen->clk_cm17, 1, 1);
	}
}

static uint32_t
cm2_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	return cgen->reg_cm2;
}

static void
cm2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	uint8_t diff = value ^ cgen->reg_cm2;
	if (SigNode_Val(cgen->sigPRC0) == SIG_LOW) {
		fprintf(stderr, "%s: write protected by PRC0\n", __func__);
		return;
	}
	cgen->reg_cm2 = value;
	if (diff & CM2_1) {
		update_cm21(cgen);
	}
}

static void
update_plc(M32C_Sysco * cgen)
{
	//uint16_t plc = cgen->reg_plc0 | ((uint16_t)cgen->reg_plc1) << 16;
	int mul, div;
	div = (cgen->reg_plc1 & (1 << 2)) ? 3 : 2;
	mul = 2 * (cgen->reg_plc0 & 7);
	if ((mul != 6) && (mul != 8)) {
		fprintf(stderr, "Bad PLL clock multiplicator\n");
		return;
	}
	Clock_MakeDerived(cgen->clk_fpll, cgen->clk_main, mul, div);
}

static uint32_t
plc0_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	return cgen->reg_plc0;
}

static void
plc0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	uint8_t diff = value ^ cgen->reg_plc0;
	if (SigNode_Val(cgen->sigPRC0) == SIG_LOW) {
		fprintf(stderr, "%s: write protected by PRC0\n", __func__);
		return;
	}
	cgen->reg_plc0 = value;
	if (diff) {
		update_plc(cgen);
	}
}

static uint32_t
plc1_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	return cgen->reg_plc1;
}

static void
plc1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

	M32C_Sysco *cgen = clientData;
	uint8_t diff = value ^ cgen->reg_plc1;
	if (SigNode_Val(cgen->sigPRC0) == SIG_LOW) {
		fprintf(stderr, "%s: write protected by PRC0\n", __func__);
		return;
	}
	cgen->reg_plc1 = value;
	if (diff) {
		update_plc(cgen);
	}
}

static void
update_pm2(M32C_Sysco * cgen)
{
	uint8_t pm25 = !!(cgen->reg_pm2 & PM2_5);
	uint8_t pm2_67 = (cgen->reg_pm2 >> 6) & 3;
	uint8_t pm2_2 = (cgen->reg_pm2 >> 2) & 1;
	uint8_t pm2_4 = (cgen->reg_pm2 >> 4) & 1;
	if (pm25) {
		Clock_MakeDerived(cgen->clk_can, cgen->clk_fcan, 1, 1);
	} else {
		Clock_MakeDerived(cgen->clk_can, cgen->clk_f1, 1, 1);
	}
	if (pm2_4) {
		Clock_MakeDerived(cgen->clk_pm24, cgen->clk_main, 1, 1);
		Clock_MakeDerived(cgen->clk_fcpu, cgen->clk_main, 1, 1);
	} else {
		Clock_MakeDerived(cgen->clk_pm24, cgen->clk_cm07, 1, 1);
		Clock_MakeDerived(cgen->clk_fcpu, cgen->clk_cm07, 1, 1);
	}
	if (pm2_2) {
		Clock_MakeDerived(cgen->clk_wdt, cgen->clk_froc, 1, 1);
	} else {
		Clock_MakeDerived(cgen->clk_wdt, cgen->clk_wdtPre, 1, 1);
	}
	switch (pm2_67) {
	    case 0:
		    Clock_MakeDerived(cgen->clk_f2n, cgen->clk_cm21, 1, 2);
		    break;
	    case 1:
		    Clock_MakeDerived(cgen->clk_f2n, cgen->clk_xin, 1, 2);
		    break;
	    case 2:
		    Clock_MakeDerived(cgen->clk_f2n, cgen->clk_froc, 1, 2);
		    break;
	    case 3:
		    Clock_MakeDerived(cgen->clk_f2n, cgen->clk_froc, 0, 2);
		    break;
	}
}

static uint32_t
pm2_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	return cgen->reg_pm2;
}

static void
pm2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
	uint8_t diff = value ^ cgen->reg_pm2;
	cgen->reg_pm2 = value | (cgen->reg_pm2 & 0x6);
	if (diff) {
		update_pm2(cgen);
	}
}

static uint32_t
tcspr_read(void *clientData, uint32_t address, int rqlen)
{
	return 0;
}

static void
tcspr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{

}

static void
update_wdtPreClk(M32C_Sysco *cgen)
{
    uint32_t divider;
    if (cgen->reg_wdc & WDC_WDC7) {
        divider = 128;
    } else {
        divider = 16;
    }
    Clock_MakeDerived(cgen->clk_wdtPre, cgen->clk_fcpu, 1, divider);
}

static uint32_t
wdc_read(void *clientData, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
    wdtActualizeCounter(cgen);
    return (cgen->reg_wdc & 0xe0) | ((cgen->wdtCounter & 0x7c00) >> 10);
}

static void
wdc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	M32C_Sysco *cgen = clientData;
    wdtActualizeCounter(cgen);
    cgen->reg_wdc = value & 0xe0;
    cgen->reg_wdc |= WDC_WDC5;
    update_wdtPreClk(cgen);
    wdtUpdateTimeout(cgen);
}

static uint32_t
wdts_read(void *clientData, uint32_t address, int rqlen)
{
    return 0;
}

static void
wdts_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
    M32C_Sysco *sysco = clientData;
    wdtActualizeCounter(sysco);
    sysco->wdtCounter = 0x7fff;
    wdtUpdateTimeout(sysco);
}

static void
M32CSysco_Unmap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete8(SYS_PM1);
	IOH_Delete8(CG_CM0);
	IOH_Delete8(CG_CM1);
	IOH_Delete8(CG_MCD);
	IOH_Delete8(CG_CM2);
	IOH_Delete8(CG_PLC0);
	IOH_Delete8(CG_PLC1);
	IOH_Delete8(CG_PM2);
	IOH_Delete8(CG_TCSPR);
	IOH_Delete8(REG_WDC);
}

static void
M32CSysco_Map(void *owner, uint32_t base, uint32_t mask, uint32_t mapflags)
{
	M32C_Sysco *cgen = (M32C_Sysco *) owner;
    uint32_t flags = IOH_FLG_HOST_ENDIAN | IOH_FLG_OSZR_NEXT | IOH_FLG_OSZW_NEXT;
	IOH_New8(SYS_PM1, pm1_read, pm1_write, cgen);
	IOH_New8(CG_CM0, cm0_read, cm0_write, cgen);
	IOH_New8(CG_CM1, cm1_read, cm1_write, cgen);
	IOH_New8(CG_MCD, mcd_read, mcd_write, cgen);
	IOH_New8(CG_CM2, cm2_read, cm2_write, cgen);
	IOH_New8(CG_PLC0, plc0_read, plc0_write, cgen);
	IOH_New8(CG_PLC1, plc1_read, plc1_write, cgen);
	IOH_New8(CG_PM2, pm2_read, pm2_write, cgen);
	IOH_New8(CG_TCSPR, tcspr_read, tcspr_write, cgen);

    IOH_New8f(REG_WDTS, wdts_read, wdts_write, cgen, flags);
    IOH_New8f(REG_WDC, wdc_read, wdc_write, cgen, flags);
}

BusDevice *
M32C_SyscoNew(const char *name)
{
	M32C_Sysco *cgen = sg_new(M32C_Sysco);
	uint32_t clock_xin;
	cgen->bdev.first_mapping = NULL;
	cgen->bdev.Map = M32CSysco_Map;
	cgen->bdev.UnMap = M32CSysco_Unmap;
	cgen->bdev.owner = cgen;
	cgen->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;

    CycleTimer_Init(&cgen->wdtTimer, wdt_timeout, cgen);

	cgen->clk_xin = Clock_New("%s.xin", name);;
	cgen->clk_xcin = Clock_New("%s.xcin", name);;
	cgen->clk_fc = Clock_New("%s.fc", name);
	cgen->clk_fcan = Clock_New("%s.fcan", name);
	cgen->clk_can = Clock_New("%s.can", name);
	cgen->clk_fpfc = Clock_New("%s.fpfc", name);
	cgen->clk_main = Clock_New("%s.main", name);
	cgen->clk_fpll = Clock_New("%s.fpll", name);
	cgen->clk_cm17 = Clock_New("%s.cm17", name);
	cgen->clk_cm21 = Clock_New("%s.cm21", name);
	cgen->clk_mcd = Clock_New("%s.mcd", name);
	cgen->clk_cm07 = Clock_New("%s.cm07", name);
	cgen->clk_pm24 = Clock_New("%s.pm24", name);
	cgen->clk_fcpu = Clock_New("%s.fcpu", name);
	cgen->clk_froc = Clock_New("%s.froc", name);

	cgen->clk_fad = Clock_New("%s.fad", name);
	cgen->clk_f1 = Clock_New("%s.f1", name);
	cgen->clk_f8 = Clock_New("%s.f8", name);
	cgen->clk_f32 = Clock_New("%s.f32", name);
	cgen->clk_fc32 = Clock_New("%s.fc32", name);
	cgen->clk_f2n = Clock_New("%s.f2n", name);
	cgen->clk_wdt = Clock_New("%s.wdt", name);
	cgen->clk_wdtPre = Clock_New("%s.wdt_pre", name); /* Internal wdt signal behind the prescaler */

	clock_xin = 8000000;
	Config_ReadUInt32(&clock_xin, name, "xin");
	Clock_SetFreq(cgen->clk_xin, clock_xin);
	Clock_SetFreq(cgen->clk_froc, 1000000);	/* Should be connected instead */
	Clock_SetFreq(cgen->clk_xcin, 0);	/* Should be connected instead */
	Clock_MakeDerived(cgen->clk_fcan, cgen->clk_main, 1, 1);
	Clock_MakeDerived(cgen->clk_main, cgen->clk_xin, 1, 1);
	Clock_MakeDerived(cgen->clk_fc, cgen->clk_xcin, 1, 1);
	Clock_MakeDerived(cgen->clk_fpfc, cgen->clk_cm21, 1, 1);
	Clock_MakeDerived(cgen->clk_fad, cgen->clk_fpfc, 1, 1);
	Clock_MakeDerived(cgen->clk_f1, cgen->clk_fpfc, 1, 1);
	Clock_MakeDerived(cgen->clk_f8, cgen->clk_fpfc, 1, 8);
	Clock_MakeDerived(cgen->clk_f32, cgen->clk_fpfc, 1, 32);
	Clock_MakeDerived(cgen->clk_fc32, cgen->clk_xcin, 1, 32);

	cgen->reg_cm0 = 8;
	cgen->reg_cm1 = 0x20;
	cgen->reg_mcd = 8;
	cgen->reg_cm2 = 0;
	cgen->reg_plc0 = 0x12;
	cgen->reg_plc1 = 0x00;
	cgen->reg_pm2 = 0x00;
	cgen->reg_tcspr = 0x00;
	cgen->sigPRC0 = SigNode_New("%s.prc0", name);
	if (!cgen->sigPRC0) {
		fprintf(stderr, "Can't create sig PRC0\n");
		exit(1);
	}
	SigNode_Set(cgen->sigPRC0, SIG_PULLDOWN);
	update_cm17(cgen);
	update_cm07(cgen);
	update_mcd(cgen);
	update_cm21(cgen);
    update_wdtPreClk(cgen);
	update_pm2(cgen);
    cgen->wdtCounter = 0;
    //update_wdt_clock(wdt);

	fprintf(stderr, "M32C Clock Generator \"%s\" created\n", name);
	return &cgen->bdev;
}
