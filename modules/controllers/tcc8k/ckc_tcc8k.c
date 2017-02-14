/**
 * TCC8000 clock controller module
 */

#include <unistd.h>
#include "bus.h"
#include "signode.h"
#include "sgstring.h"
#include "uart_tcc8k.h"
#include "serial.h"
#include "cycletimer.h"
#include "clock.h"

#define REG_CLKCTRL(base) 	((base) + 0x00)
#define REG_PLL0CFG(base) 	((base) + 0x04)
#define REG_PLL1CFG(base)	((base) + 0x08)
#define REG_CLKDIVC0(base) 	((base) + 0x0C)
#define REG_MODECTR(base) 	((base) + 0x10)
#define REG_BCLKCTR0(base) 	((base) + 0x14)
#define REG_SWRESET0(base) 	((base) + 0x18)
//REG_PCLKCFGx(base)    0x1C ~ 0x50 R/W - Reserved (TCC83xx Peri-Clock Registers)
#define REG_BCLKCTR1(base) 	((base) + 0x60)
#define REG_SWRESET1(base) 	((base) + 0x64)
#define REG_PWDCTL(base) 	((base) + 0x68)
#define REG_PLL2CFG(base) 	((base) + 0x6C)
#define REG_CLKDIVC1(base) 	((base) + 0x70)

#define CKSEL_PLL0 	(0)
#define CKSEL_PLL1 	(1)
#define CKSEL_PLL0DIV 	(2)
#define CKSEL_PLL1DIV 	(3)
#define CKSEL_XI 	(4)
#define CKSEL_XIDIV 	(5)
#define CKSEL_XTISUB 	(6)
#define CKSEL_XTIDIV 	(7)
#define CKSEL_PLL2 	(8)
#define CKSEL_PLL2DIV 	(9)
#define CKSEL_UTMI48 	(15)

#define REG_ACLKREF(base) 	((base) + 0x80)
#define REG_ACLKI2C(base) 	((base) + 0x84)
#define REG_ACLKSPI0(base) 	((base) + 0x88)
#define REG_ACLKSPI1(base) 	((base) + 0x8C)
#define REG_ACLKUART0(base) 	((base) + 0x90)
#define REG_ACLKUART1(base) 	((base) + 0x94)
#define REG_ACLKUART2(base) 	((base) + 0x98)
#define REG_ACLKUART3(base) 	((base) + 0x9C)
#define REG_ACLKUART4(base) 	((base) + 0xA0)
#define REG_ACLKTCT(base) 	((base) + 0xA4)
#define REG_ACLKTCX(base) 	((base) + 0xA8)
#define REG_ACLKTCZ(base) 	((base) + 0xAC)
#define REG_ACLKADC(base) 	((base) + 0xB0)
#define REG_ACLKDAI0(base) 	((base) + 0xB4)
#define REG_ACLKDAI1(base) 	((base) + 0xB8)
#define REG_ACLKLCD(base) 	((base) + 0xBC)
#define REG_ACLKSPDIF(base) 	((base) + 0xC0)
#define REG_ACLKUSBH(base) 	((base) + 0xC4)
#define REG_ACLKSDH0(base) 	((base) + 0xC8)
#define REG_ACLKSDH1(base) 	((base) + 0xCC)
#define REG_ACLKC3DEC(base) 	((base) + 0xD0)
#define REG_ACLKEXT(base) 	((base) + 0xD4)
#define REG_ACLKCAN0(base) 	((base) + 0xD8)
#define REG_ACLKCAN1(base)	((base) + 0xDC)
#define REG_ACLKGSB0(base) 	((base) + 0xE0)
#define REG_ACLKGSB1(base) 	((base) + 0xE4)
#define REG_ACLKGSB2(base) 	((base) + 0xE8)
#define REG_ACLKGSB3(base) 	((base) + 0xEC)

typedef struct TCC_Ckc {
	BusDevice bdev;
	Clock_t *clkPll0;
	Clock_t *clkPll1;
	Clock_t *clkPll0div;
	Clock_t *clkPll1div;
	Clock_t *clkXi;
	Clock_t *clkXidiv;
	Clock_t *clkXti;
	Clock_t *clkXtidiv;
	Clock_t *clkPll2;
	Clock_t *clkPll2div;
	Clock_t *clkUtmi48;

	Clock_t *clkSys;
	Clock_t *clkBus;
	Clock_t *clkCpu;

	Clock_t *clkRef;
	Clock_t *clkI2c;
	Clock_t *clkSpi0;
	Clock_t *clkSpi1;
	Clock_t *clkUart0;
	Clock_t *clkUart1;
	Clock_t *clkUart2;
	Clock_t *clkUart3;
	Clock_t *clkUart4;
	Clock_t *clkTct;
	Clock_t *clkTcx;
	Clock_t *clkTcz;
	Clock_t *clkDai0;
	Clock_t *clkDai1;
	Clock_t *clkAdc;
	Clock_t *clkLcd;
	Clock_t *clkSpdif;
	Clock_t *clkUsbh;
	Clock_t *clkSdh0;
	Clock_t *clkSdh1;
	Clock_t *clkC3dec;
	Clock_t *clkExt;
	Clock_t *clkCan0;
	Clock_t *clkCan1;
	Clock_t *clkGsb0;
	Clock_t *clkGsb1;
	Clock_t *clkGsb2;
	Clock_t *clkGsb3;

	uint32_t regClkctrl;
	uint32_t regPll0cfg;
	uint32_t regPll1cfg;
	uint32_t regClkdivc0;
	uint32_t regModectr;
	uint32_t regBclkctr0;
	uint32_t regSwreset0;
	uint32_t regBclkctr1;
	uint32_t regregSwreset1;
	uint32_t regPwdctl;
	uint32_t regPll2cfg;
	uint32_t regClkdivc1;
	uint32_t regAclkref;
	uint32_t regAclki2c;
	uint32_t regAclkspi0;
	uint32_t regAclkspi1;
	uint32_t regAclkuart0;
	uint32_t regAclkuart1;
	uint32_t regAclkuart2;
	uint32_t regAclkuart3;
	uint32_t regAclkuart4;
	uint32_t regAclktct;
	uint32_t regAclktcx;
	uint32_t regAclktcz;
	uint32_t regAclkadc;
	uint32_t regAclkdai0;
	uint32_t regAclkdai1;
	uint32_t regAclklcd;
	uint32_t regAclkspdif;
	uint32_t regAclkusbh;
	uint32_t regAclksdh0;
	uint32_t regAclksdh1;
	uint32_t regAclkc3dec;
	uint32_t regAclkext;
	uint32_t regAclkcan0;
	uint32_t regAclkcan1;
	uint32_t regAclkgsb0;
	uint32_t regAclkgsb1;
	uint32_t regAclkgsb2;
	uint32_t regAclkgsb3;
} TCC_Ckc;

static Clock_t *
GetClock_BySel(TCC_Ckc * ckc, int cksel)
{
	switch (cksel) {
	    case 0:
		    return ckc->clkPll0;
	    case 1:
		    return ckc->clkPll1;
	    case 2:
		    return ckc->clkPll0div;
	    case 3:
		    return ckc->clkPll1div;
	    case 4:
		    return ckc->clkXi;
	    case 5:
		    return ckc->clkXidiv;
	    case 6:
		    return ckc->clkXti;
	    case 7:
		    return ckc->clkXtidiv;
	    case 8:
		    return ckc->clkPll2;
	    case 9:
		    return ckc->clkPll2div;
	    case 15:
		    return ckc->clkUtmi48;
	    default:
		    fprintf(stderr, "clock source %d not implemented\n", cksel);
		    return NULL;
	}
}

static uint32_t
clkctrl_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regClkctrl;
}

static void
clkctrl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	Clock_t *clkSrc;
	int xe = (value >> 31) & 1;
	//int cb = (value >> 29) & 1;
	int sckdiv = (value >> 20) & 0x3;
	int cckdiv = (value >> 16) & 0xf;
	int bckdiv = (value >> 4) & 0xff;
	int cksel = (value & 0xf);
	int fcpu_mul = (16 + (cckdiv + 1) * (bckdiv + 1) - (cckdiv + 1));
	int fcpu_div = 16 * (bckdiv + 1);
	clkSrc = GetClock_BySel(ckc, cksel);
	if (clkSrc) {
		Clock_MakeDerived(ckc->clkSys, clkSrc, 1, sckdiv + 1);
	}
	Clock_MakeDerived(ckc->clkBus, ckc->clkSys, 1, bckdiv + 1);
	Clock_MakeDerived(ckc->clkCpu, ckc->clkSys, fcpu_mul, fcpu_div);
	ckc->regClkctrl = value;
	if (xe) {
		Clock_SetFreq(ckc->clkXi, 12000000);
	} else {
		Clock_SetFreq(ckc->clkXi, 0);
	}
	fprintf(stderr, "TCC8K CKC: Set clockcontrol to %08x\n", value);
}

static uint32_t
pll0cfg_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regPll0cfg;
}

static void
pll0cfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int pd = (value >> 31) & 1;
	int vs = (value >> 30) & 1;
	int s = (value >> 16) & 7;
	int m = (value >> 8) & 0xff;
	int p = (value & 0x3f);
	uint32_t xi_freq = Clock_Freq(ckc->clkXi);
	uint32_t fvco;
	if (p == 0) {
		p = ~0;
	}
	if (pd) {
		Clock_MakeDerived(ckc->clkPll0, ckc->clkXi, 0, 1);
	} else {
		Clock_MakeDerived(ckc->clkPll0, ckc->clkXi, m, p * (1 << s));
		fvco = m * xi_freq / p;
		if (vs) {
			if ((fvco < 432000000) || (fvco > 600000000)) {
				fprintf(stderr, "PLL0 F_VCO out of range %d\n", fvco);
				sleep(2);
			}
		} else {
			if ((fvco < 300000000) || (fvco > 432000000)) {
				fprintf(stderr, "PLL0 F_VCO out of range %d\n", fvco);
				sleep(2);
			}
		}
	}
	ckc->regPll0cfg = value;
	fprintf(stderr, "Written PLL0CFG to %08x\n", value);
}

static uint32_t
pll1cfg_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regPll1cfg;
}

static void
pll1cfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int pd = (value >> 31) & 1;
	int vs = (value >> 30) & 1;
	int s = (value >> 16) & 7;
	int m = (value >> 8) & 0xff;
	int p = (value & 0x3f);
	uint32_t xi_freq = Clock_Freq(ckc->clkXi);
	uint32_t fvco;
	if (p == 0) {
		p = ~0;
	}
	if (pd) {
		Clock_MakeDerived(ckc->clkPll1, ckc->clkXi, 0, 1);
	} else {
		Clock_MakeDerived(ckc->clkPll1, ckc->clkXi, m, p * (1 << s));
		fvco = m * xi_freq / p;
		if (vs) {
			if ((fvco < 432000000) || (fvco > 600000000)) {
				fprintf(stderr, "PLL1 F_VCO out of high range: %d\n", fvco);
				sleep(2);
			}
		} else {
			if ((fvco < 300000000) || (fvco > 432000000)) {
				fprintf(stderr, "PLL1 F_VCO out of low range %d\n", fvco);
				sleep(2);
			}
		}
	}
	fprintf(stderr, "PLL1 write to 0x%04x\n", value);
	ckc->regPll1cfg = value;
}

static uint32_t
clkdivc0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regClkdivc0;
}

static void
clkdivc0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int p0e = (value >> 31) & 1;
	int p1e = (value >> 23) & 1;
	int p0div = (value >> 24) & 0x3f;
	int p1div = (value >> 16) & 0x3f;
	int xe = (value >> 15) & 0x1;
	int xdiv = (value >> 8) & 0x3f;
	int xte = (value >> 7) & 1;
	int xtdiv = (value) & 0x3f;
	if (p0e) {
		Clock_MakeDerived(ckc->clkPll0div, ckc->clkPll0, 1, p0div + 1);
	} else {
		Clock_MakeDerived(ckc->clkPll0div, ckc->clkPll0, 1, 1);
	}
	if (p1e) {
		Clock_MakeDerived(ckc->clkPll1div, ckc->clkPll1, 1, p1div + 1);
	} else {
		Clock_MakeDerived(ckc->clkPll1div, ckc->clkPll1, 1, 1);
	}
	if (xe) {
		Clock_MakeDerived(ckc->clkXidiv, ckc->clkXi, 1, xdiv + 1);
	} else {
		Clock_MakeDerived(ckc->clkXidiv, ckc->clkXi, 1, 1);
	}
	if (xte) {
		Clock_MakeDerived(ckc->clkXtidiv, ckc->clkXti, 1, xtdiv + 1);
	} else {
		Clock_MakeDerived(ckc->clkXtidiv, ckc->clkXti, 1, 1);
	}
	ckc->regClkdivc0 = value;
	fprintf(stderr, "TCC8K CKC: %s: Write 0x%08x\n", __func__, value);
}

static uint32_t
modectr_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
	return 0;
}

static void
modectr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
}

static uint32_t
bclkctr0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
	return ckc->regBclkctr0;
}

static void
bclkctr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	fprintf(stderr, "BUSCLKCTR0: %08x\n", value);
	ckc->regBclkctr0 = value;
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
}

static uint32_t
swreset0_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
	return 0;
}

static void
swreset0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
}

static uint32_t
bclkctr1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
	return 0;
}

static void
bclkctr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
}

static uint32_t
swreset1_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
	return 0;
}

static void
swreset1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
}

static uint32_t
pwdctl_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
	return 0;
}

static void
pwdctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
}

static uint32_t
pll2cfg_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regPll2cfg;
}

static void
pll2cfg_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int pd = (value >> 31) & 1;
	int vs = (value >> 30) & 1;
	int s = (value >> 16) & 7;
	int m = (value >> 8) & 0xff;
	int p = (value & 0x3f);
	uint32_t xi_freq = Clock_Freq(ckc->clkXi);
	uint32_t fvco;
	if (p == 0) {
		p = ~0;
	}
	if (pd) {
		Clock_MakeDerived(ckc->clkPll2, ckc->clkXi, 0, 1);
		fprintf(stderr, "Power down\n");
	} else {
		Clock_MakeDerived(ckc->clkPll2, ckc->clkXi, m, p * (1 << s));
		fvco = m * xi_freq / p;
		if (vs) {
			if ((fvco < 432000000) || (fvco > 600000000)) {
				fprintf(stderr, "PLL F_VCO out of range %u, xi %u\n", fvco,
					xi_freq);
				sleep(2);
			} else {
				fprintf(stderr, "PLL F_VCO good\n");
			}
		} else {
			if ((fvco < 300000000) || (fvco > 432000000)) {
				fprintf(stderr, "PLL F_VCO out of range %u, xi %u\n", fvco,
					xi_freq);
				sleep(2);
			} else {
				fprintf(stderr, "PLL F_VCO good\n");
			}
		}
	}
	ckc->regPll2cfg = value;
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
}

static uint32_t
clkdivc1_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regClkdivc1;
}

static void
clkdivc1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int p2e = (value >> 7) & 1;
	int p2div = (value >> 0) & 0x3f;
	if (p2e) {
		Clock_MakeDerived(ckc->clkPll2div, ckc->clkPll2, 1, p2div + 1);
	} else {
		Clock_MakeDerived(ckc->clkPll2div, ckc->clkPll2, 1, 1);
	}
	ckc->regClkdivc1 = value;
	fprintf(stderr, "TCC8K CKC: %s: Register not tested\n", __func__);
}

static uint32_t
aclkref_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkref;
}

static void
aclkref_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkref = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkRef, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkRef, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclki2c_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclki2c;
}

static void
aclki2c_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclki2c = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkI2c, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkI2c, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkspi0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkspi0;
}

static void
aclkspi0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkspi0 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkSpi0, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkSpi0, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkspi1_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkspi1;
}

static void
aclkspi1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkspi1 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkSpi1, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkSpi1, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkuart0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	//fprintf(stderr,"ACLK uart0 READ 0x%08x\n",ckc->regAclkuart0);
	return ckc->regAclkuart0;
}

#include "arm/arm9cpu.h"
static void
aclkuart0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkuart0 = value;
	//fprintf(stderr,"ACLK uart0 write to 0x%08x\n",value);
	if (!cken) {
		Clock_MakeDerived(ckc->clkUart0, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkUart0, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkuart1_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkuart1;
}

static void
aclkuart1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkuart1 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkUart1, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkUart1, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkuart2_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkuart2;
}

static void
aclkuart2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkuart2 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkUart2, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkUart2, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkuart3_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkuart3;
}

static void
aclkuart3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkuart3 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkUart3, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkUart3, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkuart4_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkuart4;
}

static void
aclkuart4_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkuart4 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkUart4, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkUart4, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclktct_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclktct;
}

static void
aclktct_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclktct = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkTct, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkTct, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclktcx_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclktcx;
}

static void
aclktcx_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclktcx = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkTcx, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkTcx, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclktcz_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclktcz;
}

static void
aclktcz_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclktcx = value;
	//fprintf(stderr,"Write TCZ to %08x\n",value);
	if (!cken) {
		Clock_MakeDerived(ckc->clkTcz, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkTcz, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkadc_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkadc;
}

static void
aclkadc_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkadc = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkAdc, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkAdc, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkdai0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkdai0;
}

static void
aclkdai0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkdai0 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkDai0, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkDai0, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
	fprintf(stderr, "TCC8K CKC: %s: Register incomplete\n", __func__);
}

static uint32_t
aclkdai1_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkdai1;
}

static void
aclkdai1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkdai1 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkDai1, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkDai1, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
	fprintf(stderr, "TCC8K CKC: %s: Register incomplete\n", __func__);
}

static uint32_t
aclklcd_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclklcd;
}

static void
aclklcd_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclklcd = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkLcd, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkLcd, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkspdif_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkspdif;
}

static void
aclkspdif_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkspdif = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkSpdif, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkSpdif, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
	fprintf(stderr, "TCC8K CKC: %s: Register not implemented\n", __func__);
}

static uint32_t
aclkusbh_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkusbh;
}

static void
aclkusbh_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkusbh = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkUsbh, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkUsbh, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclksdh0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclksdh0;
}

static void
aclksdh0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclksdh0 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkSdh0, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkSdh0, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
	fprintf(stderr, "%s %08x\n", __func__, value);
}

static uint32_t
aclksdh1_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclksdh1;
}

static void
aclksdh1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclksdh1 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkSdh1, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkSdh1, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkc3dec_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkc3dec;
}

static void
aclkc3dec_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkc3dec = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkC3dec, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkC3dec, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkext_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkext;
}

static void
aclkext_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkext = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkExt, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkExt, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkcan0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkcan0;
}

static void
aclkcan0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkcan0 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkCan0, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkCan0, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
	//fprintf(stderr,"ACLKCAN0 %08x cksel %d, div + 1: %d\n",value,cksel,div);
	//sleep(3);
}

static uint32_t
aclkcan1_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkcan1;
}

static void
aclkcan1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkcan1 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkCan1, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkCan1, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkgsb0_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkgsb0;
}

static void
aclkgsb0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkgsb0 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkGsb0, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkGsb0, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkgsb1_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkgsb1;
}

static void
aclkgsb1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkgsb1 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkGsb1, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkGsb1, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkgsb2_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkgsb2;
}

static void
aclkgsb2_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkgsb2 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkGsb2, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkGsb2, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static uint32_t
aclkgsb3_read(void *clientData, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	return ckc->regAclkgsb3;
}

static void
aclkgsb3_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	TCC_Ckc *ckc = clientData;
	int cksel = (value >> 24) & 0xf;
	int div = value & 0xfff;
	int cken = (value >> 28) & 1;
	Clock_t *clkSrc;
	clkSrc = GetClock_BySel(ckc, cksel);
	ckc->regAclkgsb3 = value;
	if (!cken) {
		Clock_MakeDerived(ckc->clkGsb3, clkSrc, 0, 1);
	} else if (clkSrc) {
		Clock_MakeDerived(ckc->clkGsb3, clkSrc, 1, div + 1);
	} else {
		fprintf(stderr, "%s %08x\n", __func__, value);
	}
}

static void
TCkc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	TCC_Ckc *ckc = owner;
	IOH_New32(REG_CLKCTRL(base), clkctrl_read, clkctrl_write, ckc);
	IOH_New32(REG_PLL0CFG(base), pll0cfg_read, pll0cfg_write, ckc);
	IOH_New32(REG_PLL1CFG(base), pll1cfg_read, pll1cfg_write, ckc);
	IOH_New32(REG_CLKDIVC0(base), clkdivc0_read, clkdivc0_write, ckc);
	IOH_New32(REG_MODECTR(base), modectr_read, modectr_write, ckc);
	IOH_New32(REG_BCLKCTR0(base), bclkctr0_read, bclkctr0_write, ckc);
	IOH_New32(REG_SWRESET0(base), swreset0_read, swreset0_write, ckc);

	IOH_New32(REG_BCLKCTR1(base), bclkctr1_read, bclkctr1_write, ckc);
	IOH_New32(REG_SWRESET1(base), swreset1_read, swreset1_write, ckc);
	IOH_New32(REG_PWDCTL(base), pwdctl_read, pwdctl_write, ckc);
	IOH_New32(REG_PLL2CFG(base), pll2cfg_read, pll2cfg_write, ckc);
	IOH_New32(REG_CLKDIVC1(base), clkdivc1_read, clkdivc1_write, ckc);
	IOH_New32(REG_ACLKREF(base), aclkref_read, aclkref_write, ckc);
	IOH_New32(REG_ACLKI2C(base), aclki2c_read, aclki2c_write, ckc);
	IOH_New32(REG_ACLKSPI0(base), aclkspi0_read, aclkspi0_write, ckc);
	IOH_New32(REG_ACLKSPI1(base), aclkspi1_read, aclkspi1_write, ckc);
	IOH_New32(REG_ACLKUART0(base), aclkuart0_read, aclkuart0_write, ckc);
	IOH_New32(REG_ACLKUART1(base), aclkuart1_read, aclkuart1_write, ckc);
	IOH_New32(REG_ACLKUART2(base), aclkuart2_read, aclkuart2_write, ckc);
	IOH_New32(REG_ACLKUART3(base), aclkuart3_read, aclkuart3_write, ckc);
	IOH_New32(REG_ACLKUART4(base), aclkuart4_read, aclkuart4_write, ckc);
	IOH_New32(REG_ACLKTCT(base), aclktct_read, aclktct_write, ckc);
	IOH_New32(REG_ACLKTCX(base), aclktcx_read, aclktcx_write, ckc);
	IOH_New32(REG_ACLKTCZ(base), aclktcz_read, aclktcz_write, ckc);
	IOH_New32(REG_ACLKADC(base), aclkadc_read, aclkadc_write, ckc);
	IOH_New32(REG_ACLKDAI0(base), aclkdai0_read, aclkdai0_write, ckc);
	IOH_New32(REG_ACLKDAI1(base), aclkdai1_read, aclkdai1_write, ckc);
	IOH_New32(REG_ACLKLCD(base), aclklcd_read, aclklcd_write, ckc);
	IOH_New32(REG_ACLKSPDIF(base), aclkspdif_read, aclkspdif_write, ckc);
	IOH_New32(REG_ACLKUSBH(base), aclkusbh_read, aclkusbh_write, ckc);
	IOH_New32(REG_ACLKSDH0(base), aclksdh0_read, aclksdh0_write, ckc);
	IOH_New32(REG_ACLKSDH1(base), aclksdh1_read, aclksdh1_write, ckc);
	IOH_New32(REG_ACLKC3DEC(base), aclkc3dec_read, aclkc3dec_write, ckc);
	IOH_New32(REG_ACLKEXT(base), aclkext_read, aclkext_write, ckc);
	IOH_New32(REG_ACLKCAN0(base), aclkcan0_read, aclkcan0_write, ckc);
	IOH_New32(REG_ACLKCAN1(base), aclkcan1_read, aclkcan1_write, ckc);
	IOH_New32(REG_ACLKGSB0(base), aclkgsb0_read, aclkgsb0_write, ckc);
	IOH_New32(REG_ACLKGSB1(base), aclkgsb1_read, aclkgsb1_write, ckc);
	IOH_New32(REG_ACLKGSB2(base), aclkgsb2_read, aclkgsb2_write, ckc);
	IOH_New32(REG_ACLKGSB3(base), aclkgsb3_read, aclkgsb3_write, ckc);
}

static void
TCkc_UnMap(void *owner, uint32_t base, uint32_t mask)
{

	IOH_Delete32(REG_CLKCTRL(base));
	IOH_Delete32(REG_PLL0CFG(base));
	IOH_Delete32(REG_PLL1CFG(base));
	IOH_Delete32(REG_CLKDIVC0(base));
	IOH_Delete32(REG_MODECTR(base));
	IOH_Delete32(REG_BCLKCTR0(base));
	IOH_Delete32(REG_SWRESET0(base));

	IOH_Delete32(REG_BCLKCTR1(base));
	IOH_Delete32(REG_SWRESET1(base));
	IOH_Delete32(REG_PWDCTL(base));
	IOH_Delete32(REG_PLL2CFG(base));
	IOH_Delete32(REG_CLKDIVC1(base));
	IOH_Delete32(REG_ACLKREF(base));
	IOH_Delete32(REG_ACLKI2C(base));
	IOH_Delete32(REG_ACLKSPI0(base));
	IOH_Delete32(REG_ACLKSPI1(base));
	IOH_Delete32(REG_ACLKUART0(base));
	IOH_Delete32(REG_ACLKUART1(base));
	IOH_Delete32(REG_ACLKUART2(base));
	IOH_Delete32(REG_ACLKUART3(base));
	IOH_Delete32(REG_ACLKUART4(base));
	IOH_Delete32(REG_ACLKTCT(base));
	IOH_Delete32(REG_ACLKTCX(base));
	IOH_Delete32(REG_ACLKTCZ(base));
	IOH_Delete32(REG_ACLKADC(base));
	IOH_Delete32(REG_ACLKDAI0(base));
	IOH_Delete32(REG_ACLKDAI1(base));
	IOH_Delete32(REG_ACLKLCD(base));
	IOH_Delete32(REG_ACLKSPDIF(base));
	IOH_Delete32(REG_ACLKUSBH(base));
	IOH_Delete32(REG_ACLKSDH0(base));
	IOH_Delete32(REG_ACLKSDH1(base));
	IOH_Delete32(REG_ACLKC3DEC(base));
	IOH_Delete32(REG_ACLKEXT(base));
	IOH_Delete32(REG_ACLKCAN0(base));
	IOH_Delete32(REG_ACLKCAN1(base));
	IOH_Delete32(REG_ACLKGSB0(base));
	IOH_Delete32(REG_ACLKGSB1(base));
	IOH_Delete32(REG_ACLKGSB2(base));
	IOH_Delete32(REG_ACLKGSB3(base));
	//     IOH_Delete32(UART_RBR(base));

}

BusDevice *
TCC8K_CKCNew(const char *name)
{
	TCC_Ckc *ckc = sg_new(TCC_Ckc);
	ckc->bdev.first_mapping = NULL;
	ckc->bdev.Map = TCkc_Map;
	ckc->bdev.UnMap = TCkc_UnMap;
	ckc->bdev.owner = ckc;
	ckc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;

	ckc->clkPll0 = Clock_New("%s.clkPll0", name);
	ckc->clkPll1 = Clock_New("%s.clkPll1", name);
	ckc->clkPll0div = Clock_New("%s.clkPll0div", name);
	ckc->clkPll1div = Clock_New("%s.clkPll1div", name);
	ckc->clkXi = Clock_New("%s.clkXi", name);
	ckc->clkXidiv = Clock_New("%s.clkXidiv", name);
	ckc->clkXti = Clock_New("%s.clkXti", name);
	ckc->clkXtidiv = Clock_New("%s.clkXtidiv", name);
	ckc->clkPll2 = Clock_New("%s.clkPll2", name);
	ckc->clkPll2div = Clock_New("%s.clkPll2div", name);
	ckc->clkUtmi48 = Clock_New("%s.clkUtmi48", name);
	ckc->clkSys = Clock_New("%s.clkSys", name);
	ckc->clkBus = Clock_New("%s.clkBus", name);
	ckc->clkCpu = Clock_New("%s.clkCpu", name);

	ckc->clkRef = Clock_New("%s.clkRef", name);
	ckc->clkI2c = Clock_New("%s.clkI2c", name);
	ckc->clkSpi0 = Clock_New("%s.clkSpi0", name);
	ckc->clkSpi1 = Clock_New("%s.clkSpi1", name);
	ckc->clkUart0 = Clock_New("%s.clkUart0", name);
	ckc->clkUart1 = Clock_New("%s.clkUart1", name);
	ckc->clkUart2 = Clock_New("%s.clkUart2", name);
	ckc->clkUart3 = Clock_New("%s.clkUart3", name);
	ckc->clkUart4 = Clock_New("%s.clkUart4", name);
	ckc->clkTct = Clock_New("%s.clkTct", name);
	ckc->clkTcx = Clock_New("%s.clkTcx", name);
	ckc->clkTcz = Clock_New("%s.clkTcz", name);
	ckc->clkDai0 = Clock_New("%s.clkDai0", name);
	ckc->clkDai1 = Clock_New("%s.clkDai1", name);
	ckc->clkAdc = Clock_New("%s.clkAdc", name);
	ckc->clkLcd = Clock_New("%s.clkLcd", name);
	ckc->clkSpdif = Clock_New("%s.clkSpdif", name);
	ckc->clkUsbh = Clock_New("%s.clkUsbh", name);
	ckc->clkSdh0 = Clock_New("%s.clkSdh0", name);
	ckc->clkSdh1 = Clock_New("%s.clkSdh1", name);
	ckc->clkC3dec = Clock_New("%s.clkC3dec", name);
	ckc->clkExt = Clock_New("%s.clkExt", name);
	ckc->clkCan0 = Clock_New("%s.clkCan0", name);
	ckc->clkCan1 = Clock_New("%s.clkCan1", name);
	ckc->clkGsb0 = Clock_New("%s.clkGsb0", name);
	ckc->clkGsb1 = Clock_New("%s.clkGsb1", name);
	ckc->clkGsb2 = Clock_New("%s.clkGsb2", name);
	ckc->clkGsb3 = Clock_New("%s.clkGsb3", name);

	if (!ckc->clkPll0 || !ckc->clkPll1 || !ckc->clkPll0div || !ckc->clkPll1div ||
	    !ckc->clkXi || !ckc->clkXidiv || !ckc->clkXti || !ckc->clkXtidiv ||
	    !ckc->clkPll2 || !ckc->clkPll2div || !ckc->clkUtmi48 ||
	    !ckc->clkSys || !ckc->clkBus || !ckc->clkCpu) {
		fprintf(stderr, "Can not create clocks for Clock controller \"%s\"\n", name);
		exit(1);
	}
	if (!ckc->clkRef || !ckc->clkI2c || !ckc->clkSpi0 || !ckc->clkSpi1 || !ckc->clkUart0 ||
	    !ckc->clkUart1 || !ckc->clkUart2 || !ckc->clkUart3 || !ckc->clkUart4 || !ckc->clkTct ||
	    !ckc->clkTcx || !ckc->clkTcz || !ckc->clkAdc || !ckc->clkLcd || !ckc->clkSpdif ||
	    !ckc->clkUsbh || !ckc->clkSdh0 || !ckc->clkSdh1 || !ckc->clkC3dec || !ckc->clkExt ||
	    !ckc->clkCan0 || !ckc->clkCan1 || !ckc->clkGsb0 || !ckc->clkGsb1 || !ckc->clkGsb2 ||
	    !ckc->clkGsb3 || !ckc->clkDai0 || !ckc->clkDai1) {
		fprintf(stderr, "Can not create peripheral clocks in Clock controller \"%s\"\n",
			name);
		exit(1);
	}
	ckc->regClkctrl = 0x80000004;	/* This is the reset value of the chip */
	ckc->regClkctrl = 0x801f0011;	/* this is the value set by the Internal rom code */
	ckc->regClkctrl = 0x800f0011;	/* this is the value set by the ddr.S */
	ckc->regClkctrl = 0x800f0021;	/* this is the value set by the ddr.S */
	ckc->regClkctrl = 0x800f0011;	/* this is the value set by the ddr.S */
	clkctrl_write(ckc, ckc->regClkctrl, 0, 4);
	ckc->regPll0cfg = 0x80016003;
	ckc->regPll0cfg = 0x00016003;	/* This is the value set by irom code */
	pll0cfg_write(ckc, ckc->regPll0cfg, 0, 4);
	ckc->regPll1cfg = 0x80016003;
	ckc->regPll1cfg = 0x00016003;	/* irom */
	ckc->regPll1cfg = 0x40016e03;	/* ddr.S */
	//ckc->regPll1cfg = 0x00005003;
	ckc->regPll1cfg = 0x00006403;

	pll1cfg_write(ckc, ckc->regPll1cfg, 0, 4);
	ckc->regClkdivc0 = 0x03030303;
	clkdivc0_write(ckc, ckc->regClkdivc0, 0, 4);
	ckc->regModectr = 0x0;
	ckc->regBclkctr0 = 0x5Bffffff;
	ckc->regSwreset0 = 0;
	ckc->regBclkctr1 = 0x001fffff;
	ckc->regregSwreset1 = 0xffe00000;
	ckc->regPwdctl = 0x0;
	ckc->regPll2cfg = 0x80016003;
	pll2cfg_write(ckc, ckc->regPll2cfg, 0, 4);
	ckc->regClkdivc1 = 3;
	ckc->regAclkref = 0x140000ba;
	aclkref_write(ckc, ckc->regAclkref, 0, 4);
	ckc->regAclki2c = 0;
	ckc->regAclkspi0 = 0;
	ckc->regAclkspi1 = 0;
	ckc->regAclkuart0 = 0;
	ckc->regAclkuart1 = 0;
	ckc->regAclkuart2 = 0;
	ckc->regAclkuart3 = 0;
	ckc->regAclkuart4 = 0;
	ckc->regAclktct = 0;
	ckc->regAclktcx = 0x14000000;
	aclktcx_write(ckc, ckc->regAclktcx, 0, 4);
	ckc->regAclktcz = 0;
	ckc->regAclktcz = 0x10000000;	/* This is possibly not true but irom needs it */
	aclktcz_write(ckc, ckc->regAclktcz, 0, 4);
	ckc->regAclkadc = 0;
	ckc->regAclkdai0 = 0;
	ckc->regAclkdai1 = 0;
	ckc->regAclklcd = 0;
	ckc->regAclkspdif = 0;
	ckc->regAclkusbh = 0;
	ckc->regAclksdh0 = 0;
	ckc->regAclksdh0 = 0x10000007;	/* IROM needs this */
	aclksdh0_write(ckc, ckc->regAclksdh0, 0, 4);
	ckc->regAclksdh1 = 0;
	ckc->regAclksdh1 = 0x10000007;	/* IROM needs this */
	aclksdh1_write(ckc, ckc->regAclksdh1, 0, 4);
	ckc->regAclkc3dec = 0;
	ckc->regAclkext = 0;
	ckc->regAclkcan0 = 0;
	ckc->regAclkcan1 = 0;
	ckc->regAclkgsb0 = 0;
	ckc->regAclkgsb1 = 0;
	ckc->regAclkgsb2 = 0;
	ckc->regAclkgsb3 = 0;
	return &ckc->bdev;
}
