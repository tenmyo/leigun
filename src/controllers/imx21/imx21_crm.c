/*
 *************************************************************************************************
 *
 * Emulation of Freescale iMX21 Clock and reset controller module
 *
 * state: working, but currently no connection to clocks outside  
 *	  of this module
 *
 * Copyright 2006 Jochen Karrer. All rights reserved.
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

#include <errno.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "fio.h"
#include "signode.h"
#include "imx21_crm.h"
#include "configfile.h"
#include "clock.h"
#include "cycletimer.h"
#include "sgstring.h"

//#define CRM_BASE_ADDR (0x10027000)
#define CRM_CSCR(base)        ((base) + 0x0)
#define 	CSCR_PRESC_POS   (29)
#define 	CSCR_PRESC(n)     (((n) & 0x03) << CSCR_PRESC_POS)	/* MPU PLL clock prescaler */
#define 	CSCR_USB_DIV(n)   (((n) & 0x07) << 26)	/* Divider for CLK48M generation */
#define 	CSCR_SD_CNT(n)    (((n) & 0x03) << 24)	/* Shut-Down count */
#define 	CSCR_SPLL_RESTART (1 << 22)	/* Restarts the SPLL */
#define 	CSCR_MPLL_RESTART (1 << 21)	/* Restarts the MPLL */
#define 	CSCR_SSI2_SEL     (1 << 20)	/* SSI2 Baud Source Select */
#define 	CSCR_SSI1_SEL     (1 << 19)	/* SSI1 Baud Source Select */
#define 	CSCR_FIR_SEL      (1 << 18)	/* FIR and MIR clock select */
#define 	CSCR_SP_SEL       (1 << 17)	/* SPLL clock source */
#define 	CSCR_MCU_SEL      (1 << 16)	/* MPLL clock source */
#define 	CSCR_BCLKDIV_POS  10
#define 	CSCR_BCLKDIV(n)   (((n) & 0x0f) << CSCR_BCLKDIV_POS)	/* System Bus Clock Divider */
#define 	CSCR_IPDIV(n)     (((n) & 0x01) << 9)	/* Peripheral Clock Divider */
#define 	CSCR_OSC26M_DIS    (1 << 3)	/* Enables the 26 MHz oscillator */
#define 	CSCR_FPM_EN       (1 << 2)	/* Frequency Premultiplier Enable */
#define 	CSCR_SPEN         (1 << 1)	/* Serial Peripheral PLL Enable */
#define 	CSCR_MPEN         (1 << 0)	/* MPLL Enable */
#define CRM_MPCTL0(base)      ((base) + 0x4)
#define CRM_MPCTL1(base)      ((base) + 0x8)
#define		MPCTL1_BRMO 	(1<<6)
#define		MPCTL1_LF	(1<<15)
#define CRM_SPCTL0(base)      ((base) + 0xc)
#define CRM_SPCTL1(base)      ((base) + 0x10)
#define		SPCTL1_BRMO	(1<<6)
#define		SPCTL1_LF	(1<<15)
#define CRM_OSC26MCTL(base)   ((base) + 0x14)
#define		OSC26MCTL_AGC_SHIFT	(8)
#define		OSC26MCTL_AGC_MASK	(0x3f<<8)
#define		OSC26MCTL_PEAK_MASK	(3<<16)
#define		OSC26MCTL_PEAK_SHIFT	(16)
#define		OSC26M_PEAK_OK		(0<<16)
#define		OSC26M_PEAK_LOW		(1<<16)
#define		OSC26M_PEAK_HIGH	(2<<16)
#define		OSC26M_PEAK_INVALID	(3<<16)
#define CRM_PCDR0(base)       ((base) + 0x18)
#define CRM_PCDR1(base)       ((base) + 0x1c)
#define CRM_PCCR0(base)       ((base) + 0x20)
#define		PCCR0_HCLK_CSI_EN	(1<<31)
#define		PCCR0_HCLK_DMA_EN	(1<<30)
#define		PCCR0_HCLK_BROM_EN	(1<<28)
#define		PCCR0_HCLK_EMMA_EN	(1<<27)
#define		PCCR0_HCLK_LCDC_EN	(1<<26)
#define		PCCR0_HCLK_SLCDC_EN	(1<<25)
#define		PCCR0_HCLK_USBOTG_EN	(1<<24)
#define 	PCCR0_HCLK_BMI_EN	(1<<23)
#define		PCCR0_PERCLK4_EN	(1<<22)
#define		PCCR0_SLCDC_EN		(1<<21)
#define		PCCR0_FIRI_BAUD_EN	(1<<20)
#define		PCCR0_NFC_EN		(1<<19)
#define		PCCR0_PERCLK3_EN	(1<<18)
#define		PCCR0_SSI1_BAUD_EN	(1<<17)
#define		PCCR0_SSI2_BAUD_EN	(1<<16)
#define		PCCR0_EMMA_EN		(1<<15)
#define		PCCR0_USBOTG_EN		(1<<14)
#define		PCCR0_DMA_EN		(1<<13)
#define		PCCR0_I2C_EN		(1<<12)
#define		PCCR0_GPIO_EN		(1<<11)
#define		PCCR0_SDHC2_EN		(1<<10)
#define 	PCCR0_SDHC1_EN		(1<<9)
#define		PCCR0_FIRI_EN		(1<<8)
#define		PCCR0_SSI2_EN		(1<<7)
#define		PCCR0_SSI1_EN		(1<<6)
#define		PCCR0_CSPI2_EN		(1<<5)
#define		PCCR0_CSPI1_EN		(1<<4)
#define		PCCR0_UART4_EN		(1<<3)
#define		PCCR0_UART3_EN		(1<<2)
#define		PCCR0_UART2_EN		(1<<1)
#define		PCCR0_UART1_EN		(1<<0)
#define CRM_PCCR1(base)       ((base) + 0x24)
#define		PCCR1_OWIRE_EN	(1<<31)
#define		PCCR1_KPP_EN	(1<<30)
#define		PCCR1_RTC_EN	(1<<29)
#define		PCCR1_PWM_EN	(1<<28)
#define		PCCR1_GPT3_EN	(1<<27)
#define		PCCR1_GPT2_EN	(1<<26)
#define		PCCR1_GPT1_EN	(1<<25)
#define		PCCR1_WDT_EN	(1<<24)
#define		PCCR1_CSPI3_EN	(1<<23)
#define		PCCR1_RTIC_EN	(1<<22)
#define		PCCR1_RNGA_EN	(1<<21)

#define CRM_CCSR(base)        ((base) + 0x28)
#define		CCSR_32K_SR		(1<<15)
#define		CCSR_CLKO_SEL_MASK	(0x1f)
#define		CCSR_CLKO_SEL_SHIFT	(0)
#define		CCSR_CLKO_SEL(x)	((x) & 0x1f)
#if 1
/* CLKO pin selects to be used with CCSR_CLKO_SEL(n) */
#define 	CCSR_CLK0_CLK32		(0)
#define 	CCSR_CLKO_PREMCLK 	(1)
#define 	CCSR_CLKO_CLK26M	(2)
#define 	CCSR_CLKO_MPLLREFCLK	(3)
#define 	CCSR_CLKO_SPLLREFCLK	(4)
#define 	CCSR_CLKO_MPLLCLK	(5)
#define		CCSR_CLKO_SPLLCLK	(6)
#define 	CCSR_CLKO_FCLK		(7)
#define 	CCSR_CLKO_HCLK		(8)
#define		CCSR_CLKO_IPG_CLK	(9)
#define		CCSR_CLK0_PERCLK1	(10)
#define		CCSR_CLK0_PERCLK2	(11)
#define		CCSR_CLK0_PERCLK3	(12)
#define		CCSR_CLK0_PERCLK4	(13)
#define		CCSR_CLKO_SSI1BAUD	(14)
#define		CCSR_CLKO_SSI2BAUD	(15)
#define		CCSR_CLKO_NFCBAUD	(16)
#define		CCSR_CLKO_FIRI		(17)
#define		CCSR_CLK0_CLK48MALW	(18)
#define		CCSR_CLK0_CLK32KALW	(19)
#define		CCSR_CLK0_CLK48M	(20)
#define		CCSR_CLK0_CLK48DIV_CLKO	(21)
#endif
#define CRM_PMCTL(base)       ((base) + 0x2c)
#define CRM_PMCOUNT(base)     ((base) + 0x30)
#define CRM_WKGDCTL(base)     ((base) + 0x34)

/* 
 * System controll registers from Chapter 9 are included in the CRM module according to the
 * memory map
 */
#define	SYSCTL_SIDR0(base)	((base)+0x804)
#define	SYSCTL_SIDR1(base)	((base)+0x808)
#define	SYSCTL_SIDR2(base)	((base)+0x80c)
#define	SYSCTL_SIDR3(base)	((base)+0x810)
#define	SYSCTL_FMCR(base)	((base)+0x814)
#define SYSCTL_GPCR(base)	((base)+0x818)
#define	SYSCTL_WBCR(base)	((base)+0x81c)
#define	SYSCTL_DSCR1(base)	((base)+0x820)
#define	SYSCTL_DSCR2(base)	((base)+0x824)
#define	SYSCTL_DSCR3(base)	((base)+0x828)
#define	SYSCTL_DSCR4(base)	((base)+0x82c)
#define	SYSCTL_DSCR5(base)	((base)+0x830)
#define	SYSCTL_DSCR6(base)	((base)+0x834)
#define	SYSCTL_DSCR7(base)	((base)+0x838)
#define	SYSCTL_DSCR8(base)	((base)+0x83c)
#define	SYSCTL_DSCR9(base)	((base)+0x840)
#define	SYSCTL_DSCR10(base)	((base)+0x844)
#define	SYSCTL_DSCR11(base)	((base)+0x848)
#define	SYSCTL_DSCR12(base)	((base)+0x84c)
#define	SYSCTL_PCSR(base)	((base)+0x850)

typedef struct IMX_Crm {
	BusDevice bdev;
	char *name;
	Clock_t *osc32;
	Clock_t *osc26m;
	Clock_t *premclk;
	Clock_t *spll_refclk;
	Clock_t *mpll_refclk;
	Clock_t *mpll_clk;
	Clock_t *spll_clk;
	Clock_t *ipg_clk_32k;
	Clock_t *fclk;
	Clock_t *hclk;
	Clock_t *nfcclk;
	Clock_t *perclk1;
	Clock_t *perclk2;
	Clock_t *perclk3;
	Clock_t *perclk4;
	Clock_t *firiclk;
	Clock_t *clk48m;
	Clock_t *clk48div_clko;
	Clock_t *clko;
	Clock_t *ssi1clk;
	Clock_t *ssi2clk;
	Clock_t *perclk;	/* (ipg_clk) */
	Clock_t *firiinclk;	/* clk behind firsel */
	Clock_t *ssi1inclk;	/* clk behind ssi1sel */
	Clock_t *ssi2inclk;	/* clk behind ssi2sel */

	Clock_t *csi_hclk;
	Clock_t *dma_hclk;
	Clock_t *brom_hclk;
	Clock_t *emma_hclk;
	Clock_t *lcdc_hclk;
	Clock_t *slcdc_hclk;
	Clock_t *usbotg_hclk;
	Clock_t *bmi_hclk;

	/* PERCLKs enabled using pccr0 */
	Clock_t *slcdc_perclk;
	Clock_t *emma_perclk;
	Clock_t *usbotg_perclk;
	Clock_t *dma_perclk;
	Clock_t *i2c_perclk;
	Clock_t *gpio_perclk;
	Clock_t *sdhc2_perclk;
	Clock_t *sdhc1_perclk;
	Clock_t *firi_perclk;
	Clock_t *ssi2_perclk;
	Clock_t *ssi1_perclk;
	Clock_t *cspi2_perclk;
	Clock_t *cspi1_perclk;
	Clock_t *uart4_perclk;
	Clock_t *uart3_perclk;
	Clock_t *uart2_perclk;
	Clock_t *uart1_perclk;

	/* PERCLKs enabled using pccr1 */
	Clock_t *owire_perclk;
	Clock_t *kpp_perclk;
	Clock_t *rtc_perclk;
	Clock_t *pwm_perclk;
	Clock_t *gpt3_perclk;
	Clock_t *gpt2_perclk;
	Clock_t *gpt1_perclk;
	Clock_t *wdt_perclk;
	Clock_t *cspi3_perclk;
	Clock_t *rtic_perclk;
	Clock_t *rnga_perclk;

	uint32_t cscr;
	uint32_t mpctl0;
	uint32_t mpctl1;
	uint32_t spctl0;
	uint32_t spctl1;
	uint32_t osc26mctl;
	uint32_t pcdr0;
	uint32_t pcdr1;
	uint32_t pccr0;
	uint32_t pccr1;
	uint32_t ccsr;
	uint32_t pmctl;
	uint32_t pmcount;
	uint32_t wkgdctl;

	/* sysctl */
	uint32_t fmcr;
	uint32_t gpcr;
	uint32_t wbcr;
	uint32_t dscr[12];
	uint32_t pscr;
	CycleTimer dbgtimer;
} IMX_Crm;

static uint32_t
cscr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->cscr;
}

static void
update_mpll_clk(IMX_Crm * crm)
{
	uint32_t value = crm->mpctl0;
	unsigned int pd;
	unsigned int mfd;
	unsigned int mfi;
	unsigned int mfn;
	uint64_t mul, div;
	float fractional_part;
	int brmo;
	pd = (value >> 26) & 0xf;
	mfd = (value >> 16) & 0x3ff;
	mfi = (value >> 10) & 0xf;
	mfn = (value & 0x3ff);
	if (mfi < 5) {
		mfi = 5;
	}
	mul = 2 * (mfi * (mfd + 1) + mfn);
	div = (mfd + 1) * (pd + 1);
	if (crm->cscr & CSCR_MPEN) {
		Clock_MakeDerived(crm->mpll_clk, crm->mpll_refclk, mul, div);
	} else {
		Clock_MakeDerived(crm->mpll_clk, crm->mpll_refclk, 0, div);
	}
	/* fractional part between ]0.1..0.9[ requires second order BRMO */
	fractional_part = mfn / (mfd + 1);
	if ((fractional_part > 0.9) || (fractional_part < 0.1)) {
		brmo = 0;
	} else {
		brmo = 1;
	}
	if (brmo != !!(crm->mpctl1 & MPCTL1_BRMO)) {
		fprintf(stderr, "MPCTL warning: fractional part is %f, BRMO bit should be %d\n",
			fractional_part, brmo);
	}
	//fprintf(stderr,"MPCTL0 mul %lld, div %lld, mfn %d, mfd %d\n",mul,div,mfn,mfd);
	//fprintf(stderr,"MPCTL0 : multiplier %f, freq %f\n",(double)mul/div,Clock_Freq(crm->mpll_clk));
}

static void
update_spll_clk(IMX_Crm * crm)
{
	uint32_t value = crm->spctl0;
	unsigned int pd;
	unsigned int mfd;
	unsigned int mfi;
	unsigned int mfn;
	uint64_t mul, div;
	float fractional_part;
	int brmo;
	pd = (value >> 26) & 0xf;
	mfd = (value >> 16) & 0x3ff;
	mfi = (value >> 10) & 0xf;
	if (mfi < 5) {
		mfi = 5;
	}
	mfn = (value >> 0) & 0x3ff;
	mul = 2 * (mfi * (mfd + 1) + mfn);
	div = (mfd + 1) * (pd + 1);
	if (crm->cscr & CSCR_SPEN) {
		Clock_MakeDerived(crm->spll_clk, crm->spll_refclk, mul, div);
	} else {
		Clock_MakeDerived(crm->spll_clk, crm->spll_refclk, 0, div);
	}
	fractional_part = mfn / (mfd + 1);
	if ((fractional_part > 0.9) || (fractional_part < 0.1)) {
		brmo = 0;
	} else {
		brmo = 1;
	}
	if (brmo != !!(crm->spctl1 & SPCTL1_BRMO)) {
		fprintf(stderr, "SPCTL warning: fractional part is %f, BRMO bit should be %d\n",
			fractional_part, brmo);
	}
	//fprintf(stderr,"SPCTL0 : multiplier %f, freq %f\n",(double)mul/div,Clock_Freq(crm->spll_clk));
}

static void
cscr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	int presc = (value >> 29) & 7;
	int usb_div = (value >> 26) & 7;
	//int sd_cnt = (value >> 24) & 3;
	int spll_restart = (value >> 22) & 1;
	int mpll_restart = (value >> 21) & 1;
	int ssi2_sel = (value >> 20) & 1;
	int ssi1_sel = (value >> 19) & 1;
	int fir_sel = (value >> 18) & 1;
	int sp_sel = (value >> 17) & 1;
	int mcu_sel = (value >> 16) & 1;
	int bclkdiv = (value >> 10) & 0xf;
	int ipdiv = (value >> 9) & 1;
	int osc26m_div1p5 = (value >> 4) & 1;
	int osc26m_dis = (value >> 3) & 1;
	int fpm_en = (value >> 2) & 1;
	crm->cscr = value & 0xff7f3e1f;
	/* Should be delayed by 1-2 CLK32 cycles */
	crm->cscr &= ~(CSCR_MPLL_RESTART | CSCR_SPLL_RESTART);
	if (osc26m_dis) {
		Clock_SetFreq(crm->osc26m, 0);
	} else {
		uint32_t freq;
		if (Config_ReadUInt32(&freq, crm->name, "osc26") < 0) {
			freq = 0;
		}
		Clock_SetFreq(crm->osc26m, freq);
	}
	if (fpm_en) {
		Clock_MakeDerived(crm->premclk, crm->osc32, 512, 1);
	} else {
		Clock_MakeDerived(crm->premclk, crm->osc32, 0, 1);
	}
	if (mcu_sel) {
		if (osc26m_div1p5) {
			Clock_MakeDerived(crm->mpll_refclk, crm->osc26m, 2, 3);
		} else {
			Clock_MakeDerived(crm->mpll_refclk, crm->osc26m, 1, 1);
		}
	} else {
		Clock_MakeDerived(crm->mpll_refclk, crm->premclk, 1, 1);
	}
	if (sp_sel) {
		Clock_MakeDerived(crm->spll_refclk, crm->osc26m, 1, 1);
	} else {
		Clock_MakeDerived(crm->spll_refclk, crm->premclk, 1, 1);
	}
	update_mpll_clk(crm);
	update_spll_clk(crm);
	Clock_MakeDerived(crm->fclk, crm->mpll_clk, 1, presc + 1);
	Clock_MakeDerived(crm->hclk, crm->fclk, 1, bclkdiv + 1);
	if (fir_sel) {
		Clock_Decouple(crm->firiinclk);
		Clock_MakeDerived(crm->firiinclk, crm->spll_clk, 1, 1);
	} else {
		Clock_Decouple(crm->firiinclk);
		Clock_MakeDerived(crm->firiinclk, crm->mpll_clk, 1, 1);
	}
	if (ssi1_sel) {
		Clock_Decouple(crm->ssi1inclk);
		Clock_MakeDerived(crm->ssi1inclk, crm->mpll_clk, 1, 1);
	} else {
		Clock_Decouple(crm->ssi1inclk);
		Clock_MakeDerived(crm->ssi1inclk, crm->spll_clk, 1, 1);
	}
	if (ssi2_sel) {
		Clock_Decouple(crm->ssi2inclk);
		Clock_MakeDerived(crm->ssi2inclk, crm->mpll_clk, 1, 1);
	} else {
		Clock_Decouple(crm->ssi2inclk);
		Clock_MakeDerived(crm->ssi2inclk, crm->spll_clk, 1, 1);
	}
	Clock_MakeDerived(crm->perclk, crm->hclk, 1, ipdiv + 1);
	Clock_MakeDerived(crm->clk48m, crm->spll_clk, 1, usb_div + 1);
	if (spll_restart || mpll_restart) {
		//      Clock_DumpTree(crm->osc26m);
		//      Clock_DumpTree(crm->osc32);
	}
	return;
}

static uint32_t
mpctl0_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->mpctl0;
}

static void
mpctl0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	//fprintf(stderr,"MPCTL0: %08x\n",value);
	crm->mpctl0 = value & 0xbfff3fff;
	update_mpll_clk(crm);
	return;
}

static uint32_t
mpctl1_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->mpctl1 | MPCTL1_LF;
}

static void
mpctl1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->mpctl1 = value & (MPCTL1_BRMO);
	//fprintf(stderr,"MPCTL1: %08x\n",value);
	return;
}

static uint32_t
spctl0_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->spctl0;
}

static void
spctl0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->spctl0 = value & 0xbfff3fff;
	update_spll_clk(crm);
	//fprintf(stderr,"SPCTL0: %08x\n",value);
	return;
}

static uint32_t
spctl1_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->spctl1 | SPCTL1_LF;
}

static void
spctl1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->spctl1 = value & SPCTL1_BRMO;
	return;
}

static uint32_t
osc26mctl_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	int agc = (crm->osc26mctl & OSC26MCTL_AGC_MASK) >> OSC26MCTL_AGC_SHIFT;
	uint32_t peak;
	if ((crm->cscr & CSCR_OSC26M_DIS) || (Clock_Freq(crm->osc26m) == 0)) {
		/* verified with real board without osc26 + cscr disabled */
		peak = OSC26M_PEAK_INVALID;
	} else if (agc > 0x22) {
		peak = OSC26M_PEAK_HIGH;
	} else if (agc < 0x1a) {
		peak = OSC26M_PEAK_LOW;
	} else {
		peak = OSC26M_PEAK_OK;
	}
	//fprintf(stderr,"agc %d, peak %04x\n",agc,peak);
	crm->osc26mctl &= ~(3 << 16);
	crm->osc26mctl |= peak;	/* always in desired range */
	return crm->osc26mctl;
}

static void
osc26mctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->osc26mctl = value & 0x3f00;
	return;
}

static void
update_perdivs(IMX_Crm * crm)
{
	uint32_t pcdr0 = crm->pcdr0;
	uint32_t pcdr1 = crm->pcdr1;
	int perdiv1 = pcdr1 & 0x3f;
	int perdiv2 = (pcdr1 >> 8) & 0x3f;
	int perdiv3 = (pcdr1 >> 16) & 0x3f;
	int perdiv4 = (pcdr1 >> 24) & 0x3f;

	int ssi2div = (pcdr0 >> 26) & 0x3f;
	int ssi1div = (pcdr0 >> 16) & 0x3f;
	int nfcdiv = (pcdr0 >> 12) & 0xf;
	int clko_48mdiv = (pcdr0 >> 5) & 7;
	int firi_div = (pcdr0 >> 0) & 0x1f;

	Clock_MakeDerived(crm->clk48div_clko, crm->clk48m, 1, clko_48mdiv + 1);
	Clock_MakeDerived(crm->perclk1, crm->mpll_clk, 1, perdiv1 + 1);
	Clock_MakeDerived(crm->perclk2, crm->mpll_clk, 1, perdiv2 + 1);
	if (crm->pccr0 & PCCR0_PERCLK3_EN) {
		Clock_MakeDerived(crm->perclk3, crm->mpll_clk, 1, perdiv3 + 1);
	} else {
		Clock_MakeDerived(crm->perclk3, crm->mpll_clk, 0, perdiv3 + 1);
	}
	if (crm->pccr0 & PCCR0_PERCLK4_EN) {
		Clock_MakeDerived(crm->perclk4, crm->mpll_clk, 1, perdiv4 + 1);
	} else {
		Clock_MakeDerived(crm->perclk4, crm->mpll_clk, 0, perdiv4 + 1);
	}
	if (crm->pccr0 & PCCR0_FIRI_BAUD_EN) {
		Clock_MakeDerived(crm->firiclk, crm->firiinclk, 1, firi_div + 1);
	} else {
		Clock_MakeDerived(crm->firiclk, crm->firiinclk, 0, 1);
	}
	if (crm->pccr0 & PCCR0_NFC_EN) {
		Clock_MakeDerived(crm->nfcclk, crm->fclk, 1, nfcdiv + 1);
	} else {
		Clock_MakeDerived(crm->nfcclk, crm->fclk, 0, 1);
	}
	if (crm->pccr0 & PCCR0_SSI1_BAUD_EN) {
		if (ssi1div <= 1) {
			Clock_MakeDerived(crm->ssi1clk, crm->ssi1inclk, 1, 62);
		} else {
			Clock_MakeDerived(crm->ssi1clk, crm->ssi1inclk, 2, ssi1div);
		}
	} else {
		Clock_MakeDerived(crm->ssi1clk, crm->ssi1inclk, 0, 1);
	}
	if (crm->pccr0 & PCCR0_SSI2_BAUD_EN) {
		if (ssi2div <= 1) {
			Clock_MakeDerived(crm->ssi2clk, crm->ssi2inclk, 1, 62);
		} else {
			Clock_MakeDerived(crm->ssi2clk, crm->ssi2inclk, 2, ssi2div);
		}
	} else {
		Clock_MakeDerived(crm->ssi2clk, crm->ssi2inclk, 0, 1);
	}
	/*      fprintf(stderr,"** perclk1: freq %d\n",Clock_Freq(crm->perclk1));       */
	/*      fprintf(stderr,"** mpll_clk: freq %d\n",Clock_Freq(crm->mpll_clk));     */
}

static uint32_t
pcdr0_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->pcdr0;
}

static void
pcdr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->pcdr0 = value & 0xfc3ff0ff;
	update_perdivs(crm);
	return;
}

static uint32_t
pcdr1_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->pcdr1;
}

static void
pcdr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->pcdr1 = value & 0x3f3f3f3f;
	update_perdivs(crm);
	return;
}

static uint32_t
pccr0_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->pccr0;
}

static void
pccr0_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->pccr0 = value | (1 << 29);
	update_perdivs(crm);
	if (value & PCCR0_HCLK_CSI_EN) {
		Clock_MakeDerived(crm->csi_hclk, crm->hclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->csi_hclk, crm->hclk, 0, 1);
	}
	if (value & PCCR0_HCLK_DMA_EN) {
		Clock_MakeDerived(crm->dma_hclk, crm->hclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->dma_hclk, crm->hclk, 0, 1);
	}
	if (value & PCCR0_HCLK_BROM_EN) {
		Clock_MakeDerived(crm->brom_hclk, crm->hclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->brom_hclk, crm->hclk, 0, 1);
	}
	if (value & PCCR0_HCLK_EMMA_EN) {
		Clock_MakeDerived(crm->emma_hclk, crm->hclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->emma_hclk, crm->hclk, 0, 1);
	}
	if (value & PCCR0_HCLK_LCDC_EN) {
		Clock_MakeDerived(crm->lcdc_hclk, crm->hclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->lcdc_hclk, crm->hclk, 0, 1);
	}
	if (value & PCCR0_HCLK_SLCDC_EN) {
		Clock_MakeDerived(crm->slcdc_hclk, crm->hclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->slcdc_hclk, crm->hclk, 0, 1);
	}
	if (value & PCCR0_HCLK_USBOTG_EN) {
		Clock_MakeDerived(crm->usbotg_hclk, crm->hclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->usbotg_hclk, crm->hclk, 0, 1);
	}
	if (value & PCCR0_HCLK_BMI_EN) {
		Clock_MakeDerived(crm->bmi_hclk, crm->hclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->bmi_hclk, crm->hclk, 0, 1);
	}
	/* PERCLKs */
	if (value & PCCR0_SLCDC_EN) {
		Clock_MakeDerived(crm->slcdc_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->slcdc_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_EMMA_EN) {
		Clock_MakeDerived(crm->emma_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->emma_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_USBOTG_EN) {
		Clock_MakeDerived(crm->usbotg_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->usbotg_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_DMA_EN) {
		Clock_MakeDerived(crm->dma_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->dma_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_I2C_EN) {
		Clock_MakeDerived(crm->i2c_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->i2c_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_GPIO_EN) {
		Clock_MakeDerived(crm->gpio_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->gpio_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_SDHC2_EN) {
		Clock_MakeDerived(crm->sdhc2_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->sdhc2_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_SDHC1_EN) {
		Clock_MakeDerived(crm->sdhc1_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->sdhc1_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_FIRI_EN) {
		Clock_MakeDerived(crm->firi_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->firi_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_SSI2_EN) {
		Clock_MakeDerived(crm->ssi2_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->ssi2_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_SSI1_EN) {
		Clock_MakeDerived(crm->ssi1_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->ssi1_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_CSPI2_EN) {
		Clock_MakeDerived(crm->cspi2_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->cspi2_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_CSPI1_EN) {
		Clock_MakeDerived(crm->cspi1_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->cspi1_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_UART4_EN) {
		Clock_MakeDerived(crm->uart4_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->uart4_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_UART3_EN) {
		Clock_MakeDerived(crm->uart3_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->uart3_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_UART2_EN) {
		Clock_MakeDerived(crm->uart2_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->uart2_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR0_UART1_EN) {
		Clock_MakeDerived(crm->uart1_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->uart1_perclk, crm->perclk, 0, 1);
	}
	return;
}

static uint32_t
pccr1_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->pccr1;
}

static void
pccr1_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->pccr1 = value & 0xffe00000;
	if (value & PCCR1_OWIRE_EN) {
		Clock_MakeDerived(crm->owire_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->owire_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_KPP_EN) {
		Clock_MakeDerived(crm->kpp_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->kpp_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_RTC_EN) {
		Clock_MakeDerived(crm->rtc_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->rtc_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_PWM_EN) {
		Clock_MakeDerived(crm->pwm_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->pwm_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_GPT3_EN) {
		Clock_MakeDerived(crm->gpt3_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->gpt3_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_GPT2_EN) {
		Clock_MakeDerived(crm->gpt2_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->gpt2_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_GPT1_EN) {
		Clock_MakeDerived(crm->gpt1_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->gpt1_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_WDT_EN) {
		Clock_MakeDerived(crm->wdt_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->wdt_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_CSPI3_EN) {
		Clock_MakeDerived(crm->cspi3_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->cspi3_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_RTIC_EN) {
		Clock_MakeDerived(crm->rtic_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->rtic_perclk, crm->perclk, 0, 1);
	}
	if (value & PCCR1_RNGA_EN) {
		Clock_MakeDerived(crm->rnga_perclk, crm->perclk, 1, 1);
	} else {
		Clock_MakeDerived(crm->rnga_perclk, crm->perclk, 0, 1);
	}
	return;
}

static uint32_t
ccsr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "CCSR (0x%08x) implementation is incomplete\n", address);
	return crm->ccsr;
}

static void
ccsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	uint32_t clko_sel;
	crm->ccsr = (value & 0x1f) | (3 << 8);
	clko_sel = value & 0x1f;
	Clock_Decouple(crm->clko);
	switch (clko_sel) {
	    case CCSR_CLK0_CLK32:
		    Clock_MakeDerived(crm->clko, crm->osc32, 1, 1);
		    break;

	    case CCSR_CLKO_PREMCLK:
		    Clock_MakeDerived(crm->clko, crm->premclk, 1, 1);
		    break;

	    case CCSR_CLKO_CLK26M:
		    Clock_MakeDerived(crm->clko, crm->osc26m, 1, 1);
		    break;

	    case CCSR_CLKO_MPLLREFCLK:
		    Clock_MakeDerived(crm->clko, crm->mpll_refclk, 1, 1);
		    break;
	    case CCSR_CLKO_SPLLREFCLK:
		    Clock_MakeDerived(crm->clko, crm->spll_refclk, 1, 1);
		    break;
	    case CCSR_CLKO_MPLLCLK:
		    Clock_MakeDerived(crm->clko, crm->mpll_clk, 1, 1);
		    break;
	    case CCSR_CLKO_SPLLCLK:
		    Clock_MakeDerived(crm->clko, crm->spll_clk, 1, 1);
		    break;
	    case CCSR_CLKO_FCLK:
		    Clock_MakeDerived(crm->clko, crm->fclk, 1, 1);
		    break;
	    case CCSR_CLKO_HCLK:
		    Clock_MakeDerived(crm->clko, crm->hclk, 1, 1);
		    break;
	    case CCSR_CLKO_IPG_CLK:
		    Clock_MakeDerived(crm->clko, crm->perclk, 1, 1);
		    break;

	    case CCSR_CLK0_PERCLK1:
		    Clock_MakeDerived(crm->clko, crm->perclk1, 1, 1);
		    break;
	    case CCSR_CLK0_PERCLK2:
		    Clock_MakeDerived(crm->clko, crm->perclk2, 1, 1);
		    break;
	    case CCSR_CLK0_PERCLK3:
		    Clock_MakeDerived(crm->clko, crm->perclk3, 1, 1);
		    break;
	    case CCSR_CLK0_PERCLK4:
		    Clock_MakeDerived(crm->clko, crm->perclk4, 1, 1);
		    break;
	    case CCSR_CLKO_SSI1BAUD:
		    Clock_MakeDerived(crm->clko, crm->ssi1clk, 1, 1);
		    break;
	    case CCSR_CLKO_SSI2BAUD:
		    Clock_MakeDerived(crm->clko, crm->ssi2clk, 1, 1);
		    break;
	    case CCSR_CLKO_NFCBAUD:
		    Clock_MakeDerived(crm->clko, crm->nfcclk, 1, 1);
		    break;
	    case CCSR_CLKO_FIRI:
		    Clock_MakeDerived(crm->clko, crm->firiclk, 1, 1);
		    break;

	    case CCSR_CLK0_CLK48MALW:
		    /* What is always ??? */
		    Clock_MakeDerived(crm->clko, crm->clk48m, 1, 1);
		    break;
	    case CCSR_CLK0_CLK32KALW:
		    /* What is always ??? */
		    Clock_MakeDerived(crm->clko, crm->osc32, 1, 1);
		    break;
	    case CCSR_CLK0_CLK48M:
		    /* What is not always ??? */
		    Clock_MakeDerived(crm->clko, crm->clk48m, 1, 1);
		    break;

	    case CCSR_CLK0_CLK48DIV_CLKO:
		    Clock_MakeDerived(crm->clko, crm->clk48div_clko, 1, 1);
		    break;
	    default:
		    fprintf(stderr, "Unknown CLKO_SEL %d\n", clko_sel);
		    break;
	}
	return;
}

static uint32_t
pmctl_read(void *clientData, uint32_t address, int rqlen)
{
	// IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "read %08x not implemented\n", address);
	return 0;
}

static void
pmctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	// IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "write %08x not implemented\n", address);
	return;
}

static uint32_t
pmcount_read(void *clientData, uint32_t address, int rqlen)
{
	//  IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "read %08x not implemented\n", address);
	return 0;
}

static void
pmcount_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	// IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "write %08x not implemented\n", address);
	return;
}

static uint32_t
wkgdctl_read(void *clientData, uint32_t address, int rqlen)
{
	//  IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "read %08x not implemented\n", address);
	return 0;
}

static void
wkgdctl_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	// IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "write %08x not implemented\n", address);
	return;
}

/*
 * ------------------------------------------------------------------------------
 * System control registers from Chapter 9 
 * SIDR register: Manual is bad. Use mask revision difference pdf. 
 * Values are taken from imx21ads with 0M55B CPU
 * ------------------------------------------------------------------------------
 */
static uint32_t
sidr0_read(void *clientData, uint32_t address, int rqlen)
{
	/* GT4 Board version 1 with 1MB55 0x67837a72 */
	/* GT4 Board version 2 with 0MB55 0xf3476f6b */
	/* ADS Board version 1 with 0MB55 0xe60558ec */
	return 0xe60558ec;
}

static uint32_t
sidr1_read(void *clientData, uint32_t address, int rqlen)
{
	/* GT4 Board version 1 with 1MB55 0x00006001 */
	/* GT4 Board version 2 with 0MB55 0x00006000 */
	/* ADS Board version 1 with 0MB55 0x0000e000 */

	return 0x0000e000;
}

static uint32_t
sidr2_read(void *clientData, uint32_t address, int rqlen)
{
	/* GT4 Board version 1 with 1MB55 0x201d101d */
	/* GT4 Board version 2 with 0MB55 0x101d101d */
	/* ADS Board version 1 with 0MB55 0x101d101d */
	return 0x101d101d;
}

static uint32_t
sidr3_read(void *clientData, uint32_t address, int rqlen)
{
	/* FS3 Board version 1 with 1MB55 0xd0080000 */
	/* FS3 Board version 2 with 0MB55 0x04008000 */
	/* ADS Board version 1 with 0MB55 0x04008000 */
	return 0x40080000;
}

static void
sidr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "Silicon ID register is not writable !\n");
	return;
}

static uint32_t
fmcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->fmcr;
}

static void
fmcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "FMCR write value 0x%08x\n", value);
	crm->fmcr = value;
	return;
}

static uint32_t
gpcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "GPCR: read bootstrap pins not implemented\n");
	return crm->gpcr;
}

static void
gpcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->gpcr = value & (1 << 3);
	return;
}

static uint32_t
wbcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->wbcr;
}

static void
wbcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	crm->wbcr = value & 0xffff;
	return;
}

static uint32_t
dscr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	unsigned int index = ((address & 0xff) - 0x20) >> 2;
	if (index > 11) {
		fprintf(stderr, "emulator bug, illegal DSCR index\n");
		return 0;
	}
	fprintf(stderr, "DSCR read: value 0x%08x\n", crm->dscr[index]);
	return crm->dscr[index];
}

static void
dscr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	unsigned int index = ((address & 0xff) - 0x20) >> 2;
	if (index > 11) {
		fprintf(stderr, "emulator bug, illegal DSCR index\n");
		return;
	}
	crm->dscr[index] = value & 0x7fff7fff;
	fprintf(stderr, "DSCR%d: %08x\n", index, value);
	return;
}

static uint32_t
pscr_read(void *clientData, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	return crm->pscr;
}

static void
pscr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMX_Crm *crm = (IMX_Crm *) clientData;
	fprintf(stderr, "PSCR write 0x%08x\n", value);
	crm->pscr = value & 0x000f003f;
	return;
}

static uint32_t
undefined_read(void *clientData, uint32_t address, int rqlen)
{
	fprintf(stderr, "WARNING !: read from undefined location 0x%08x in CRM module\n", address);
	return 0;
}

static void
undefined_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "WARNING !: write to undefined location 0x%08x in CRM module\n", address);
	return;
}

static void
IMXCrm_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	IMX_Crm *crm = (IMX_Crm *) owner;

	IOH_New32(CRM_CSCR(base), cscr_read, cscr_write, crm);
	IOH_New32(CRM_MPCTL0(base), mpctl0_read, mpctl0_write, crm);
	IOH_New32(CRM_MPCTL1(base), mpctl1_read, mpctl1_write, crm);
	IOH_New32(CRM_SPCTL0(base), spctl0_read, spctl0_write, crm);
	IOH_New32(CRM_SPCTL1(base), spctl1_read, spctl1_write, crm);
	IOH_New32(CRM_OSC26MCTL(base), osc26mctl_read, osc26mctl_write, crm);
	IOH_New32(CRM_PCDR0(base), pcdr0_read, pcdr0_write, crm);
	IOH_New32(CRM_PCDR1(base), pcdr1_read, pcdr1_write, crm);
	IOH_New32(CRM_PCCR0(base), pccr0_read, pccr0_write, crm);
	IOH_New32(CRM_PCCR1(base), pccr1_read, pccr1_write, crm);
	IOH_New32(CRM_CCSR(base), ccsr_read, ccsr_write, crm);
	IOH_New32(CRM_PMCTL(base), pmctl_read, pmctl_write, crm);
	IOH_New32(CRM_PMCOUNT(base), pmcount_read, pmcount_write, crm);
	IOH_New32(CRM_WKGDCTL(base), wkgdctl_read, wkgdctl_write, crm);

	/* SIDR: 128 bit register */
	IOH_New32(SYSCTL_SIDR0(base), sidr0_read, sidr_write, crm);
	IOH_New32(SYSCTL_SIDR1(base), sidr1_read, sidr_write, crm);
	IOH_New32(SYSCTL_SIDR2(base), sidr2_read, sidr_write, crm);
	IOH_New32(SYSCTL_SIDR3(base), sidr3_read, sidr_write, crm);
	IOH_New32(SYSCTL_FMCR(base), fmcr_read, fmcr_write, crm);
	IOH_New32(SYSCTL_GPCR(base), gpcr_read, gpcr_write, crm);
	IOH_New32(SYSCTL_WBCR(base), wbcr_read, wbcr_write, crm);
	IOH_New32(SYSCTL_DSCR1(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR2(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR3(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR4(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR5(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR6(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR7(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR8(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR9(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR10(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR11(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_DSCR12(base), dscr_read, dscr_write, crm);
	IOH_New32(SYSCTL_PCSR(base), pscr_read, pscr_write, crm);
	IOH_NewRegion(base, 0x1000, undefined_read, undefined_write, IOH_FLG_HOST_ENDIAN, crm);
}

static void
IMXCrm_UnMap(void *owner, uint32_t base, uint32_t mask)
{
	IOH_Delete32(CRM_CSCR(base));
	IOH_Delete32(CRM_MPCTL0(base));
	IOH_Delete32(CRM_MPCTL1(base));
	IOH_Delete32(CRM_SPCTL0(base));
	IOH_Delete32(CRM_SPCTL1(base));
	IOH_Delete32(CRM_OSC26MCTL(base));
	IOH_Delete32(CRM_PCDR0(base));
	IOH_Delete32(CRM_PCDR1(base));
	IOH_Delete32(CRM_PCCR0(base));
	IOH_Delete32(CRM_PCCR1(base));
	IOH_Delete32(CRM_CCSR(base));
	IOH_Delete32(CRM_PMCTL(base));
	IOH_Delete32(CRM_PMCOUNT(base));
	IOH_Delete32(CRM_WKGDCTL(base));
	IOH_Delete32(SYSCTL_SIDR0(base));
	IOH_Delete32(SYSCTL_SIDR1(base));
	IOH_Delete32(SYSCTL_SIDR2(base));
	IOH_Delete32(SYSCTL_SIDR3(base));
	IOH_Delete32(SYSCTL_FMCR(base));
	IOH_Delete32(SYSCTL_GPCR(base));
	IOH_Delete32(SYSCTL_WBCR(base));
	IOH_Delete32(SYSCTL_DSCR1(base));
	IOH_Delete32(SYSCTL_DSCR2(base));
	IOH_Delete32(SYSCTL_DSCR3(base));
	IOH_Delete32(SYSCTL_DSCR4(base));
	IOH_Delete32(SYSCTL_DSCR5(base));
	IOH_Delete32(SYSCTL_DSCR6(base));
	IOH_Delete32(SYSCTL_DSCR7(base));
	IOH_Delete32(SYSCTL_DSCR8(base));
	IOH_Delete32(SYSCTL_DSCR9(base));
	IOH_Delete32(SYSCTL_DSCR10(base));
	IOH_Delete32(SYSCTL_DSCR11(base));
	IOH_Delete32(SYSCTL_DSCR12(base));
	IOH_Delete32(SYSCTL_PCSR(base));
	IOH_DeleteRegion(base, 0x1000);
}

static void
dump_proc(void *cd)
{
//      IMX_Crm *crm = (IMX_Crm*)cd;
//      Clock_DumpTree(crm->osc32);
}

BusDevice *
IMX21Crm_New(const char *name)
{
	IMX_Crm *crm = sg_new(IMX_Crm);
	uint32_t osc32_freq;

	crm->name = sg_strdup(name);
	if (!crm->name) {
		fprintf(stderr, "Out of memory for CRM name\n");
		exit(1);
	}
	crm->cscr = 0x77180607;
	crm->mpctl0 = 0x00071c07;
	crm->mpctl1 = 0x00008000;
	crm->spctl0 = 0x807f2065;
	crm->spctl1 = 0x00008000;
	crm->osc26mctl = 0x00003f00;
	crm->pcdr0 = 0x64193007;
	crm->pcdr1 = 0x0307070f;
	crm->pccr0 = 0x31084003;
	crm->pccr1 = 0x0;
	crm->ccsr = 0x300;
	crm->wkgdctl = 0;
	crm->osc32 = Clock_New("%s.clk32", name);
	crm->osc26m = Clock_New("%s.clk26", name);
	crm->premclk = Clock_New("%s.premclk", name);
	crm->spll_refclk = Clock_New("%s.spll_refclk", name);
	crm->mpll_refclk = Clock_New("%s.mpll_refclk", name);
	crm->mpll_clk = Clock_New("%s.mpll_clk", name);
	crm->spll_clk = Clock_New("%s.spll_clk", name);
	crm->ipg_clk_32k = Clock_New("%s.ipg_clk_32", name);
	crm->fclk = Clock_New("%s.fclk", name);
	crm->hclk = Clock_New("%s.hclk", name);
	crm->nfcclk = Clock_New("%s.nfcclk", name);
	crm->perclk1 = Clock_New("%s.perclk1", name);
	crm->perclk2 = Clock_New("%s.perclk2", name);
	crm->perclk3 = Clock_New("%s.perclk3", name);
	crm->perclk4 = Clock_New("%s.perclk4", name);
	crm->firiclk = Clock_New("%s.firiclk", name);
	crm->clk48m = Clock_New("%s.clk48m", name);
	crm->clk48div_clko = Clock_New("%s.clk48div_clko", name);
	crm->clko = Clock_New("%s.clko", name);
	crm->ssi1clk = Clock_New("%s.ssi1clk", name);
	crm->ssi2clk = Clock_New("%s.ssi2clk", name);
	crm->perclk = Clock_New("%s.perclk", name);	/* (ipg_clk) */
	crm->firiinclk = Clock_New("%s.firiinclk", name);
	crm->ssi1inclk = Clock_New("%s.ssi1inclk", name);
	crm->ssi2inclk = Clock_New("%s.ssi2inclk", name);

	/* HCLKs */
	crm->csi_hclk = Clock_New("%s.csi_hclk", name);
	crm->dma_hclk = Clock_New("%s.dma_hclk", name);
	crm->brom_hclk = Clock_New("%s.brom_hclk", name);
	crm->emma_hclk = Clock_New("%s.emma_hclk", name);
	crm->lcdc_hclk = Clock_New("%s.lcdc_hclk", name);
	crm->slcdc_hclk = Clock_New("%s.slcdc_hclk", name);
	crm->usbotg_hclk = Clock_New("%s.usbotg_hclk", name);
	crm->bmi_hclk = Clock_New("%s.bmi_hclk", name);
	/* PERCLKs */
	crm->slcdc_perclk = Clock_New("%s.slcdc_perclk", name);
	crm->emma_perclk = Clock_New("%s.emma_perclk", name);
	crm->usbotg_perclk = Clock_New("%s.usbotg_perclk", name);
	crm->dma_perclk = Clock_New("%s.dma_perclk", name);
	crm->i2c_perclk = Clock_New("%s.i2c_perclk", name);
	crm->gpio_perclk = Clock_New("%s.gpio_perclk", name);
	crm->sdhc2_perclk = Clock_New("%s.sdhc2_perclk", name);
	crm->sdhc1_perclk = Clock_New("%s.sdhc1_perclk", name);
	crm->firi_perclk = Clock_New("%s.firi_perclk", name);
	crm->ssi2_perclk = Clock_New("%s.ssi2_perclk", name);
	crm->ssi1_perclk = Clock_New("%s.ssi1_perclk", name);
	crm->cspi2_perclk = Clock_New("%s.cspi2_perclk", name);
	crm->cspi1_perclk = Clock_New("%s.cspi1_perclk", name);
	crm->uart4_perclk = Clock_New("%s.uart4_perclk", name);
	crm->uart3_perclk = Clock_New("%s.uart3_perclk", name);
	crm->uart2_perclk = Clock_New("%s.uart2_perclk", name);
	crm->uart1_perclk = Clock_New("%s.uart1_perclk", name);

	crm->owire_perclk = Clock_New("%s.owire_perclk", name);
	crm->kpp_perclk = Clock_New("%s.kpp_perclk", name);
	crm->rtc_perclk = Clock_New("%s.rtc_perclk", name);
	crm->pwm_perclk = Clock_New("%s.pwm_perclk", name);
	crm->gpt3_perclk = Clock_New("%s.gpt3_perclk", name);
	crm->gpt2_perclk = Clock_New("%s.gpt2_perclk", name);
	crm->gpt1_perclk = Clock_New("%s.gpt1_perclk", name);
	crm->wdt_perclk = Clock_New("%s.wdt_perclk", name);
	crm->cspi3_perclk = Clock_New("%s.cspi3_perclk", name);
	crm->rtic_perclk = Clock_New("%s.rtic_perclk", name);
	crm->rnga_perclk = Clock_New("%s.rnga_perclk", name);

	/* The 32k Osc is always running, the 26MHz is controlled by CSCR */
	if (Config_ReadUInt32(&osc32_freq, crm->name, "osc32") < 0) {
		osc32_freq = 32768;
	}
	Clock_SetFreq(crm->osc32, osc32_freq);

	crm->fmcr = 0xffffffcb;
	crm->gpcr = 0x0000000c;
	crm->wbcr = 0;
	crm->dscr[0] = 0x00400000;
	crm->pscr = 0x00000003;
	crm->bdev.first_mapping = NULL;
	crm->bdev.Map = IMXCrm_Map;
	crm->bdev.UnMap = IMXCrm_UnMap;
	crm->bdev.owner = crm;
	crm->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;

	cscr_write(crm, crm->cscr, 0, 4);
	pccr0_write(crm, crm->pccr0, 0, 4);
	pccr1_write(crm, crm->pccr1, 0, 4);
	pcdr0_write(crm, crm->pcdr0, 0, 4);
	pcdr1_write(crm, crm->pcdr1, 0, 4);
	ccsr_write(crm, crm->ccsr, 0, 4);

	fprintf(stderr, "IMX21 Clock and Reset module \"%s\" created\n", name);

	//CycleTimer_Add(&crm->dbgtimer,(uint64_t)285000000*10,dump_proc,crm);
	CycleTimer_Add(&crm->dbgtimer, (uint64_t) 3, dump_proc, crm);
	return &crm->bdev;
}
