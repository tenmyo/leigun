/*
 *************************************************************************************************
 * Emulation of Freescale i.MX21 LCD Controller 
 *
 * state: incomplete but basically working 
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
#include <termios.h>
#include <unistd.h>
#include <termios.h>
#include <sys/ioctl.h>
#include <sys/fcntl.h>
#include "bus.h"
#include "signode.h"
#include "imx21_lcdc.h"
#include "rfbserver.h"
#include "configfile.h"
#include "cycletimer.h"
#include "fbdisplay.h"
#include "clock.h"
#include "sgstring.h"

#if 0
#define dbgprintf(...) { fprintf(stderr,__VA_ARGS__); }
#else
#define dbgprintf(...)
#endif

#define LCDC_BASE_ADDR  0x10021000
#define LCDC_LSSAR(base)   ((base) + 0x00)	//  32bit lcdc screen start addr reg
#define LCDC_LSR(base)     ((base) + 0x04)	//  32bit lcdc size reg
#define		LSR_XMAX_MASK	(0x3f<<20)
#define		LSR_XMAX_SHIFT	(20)
#define		LSR_YMAX_MASK	(0x3ff)
#define		LSR_YMAX_SHIFT	(0)

#define LCDC_LVPWR(base)   ((base) + 0x08)	//  32bit lcdc virtual page width reg
#define		LVPWR_VPW_MASK	(0x3ff)
#define		LVPWR_VPW_SHIFT	(0)
#define LCDC_LCPR(base)    ((base) + 0x0C)	//  32bit lcd cursor position reg
#define		LCPR_CC_MASK	(3<<30)
#define		LCPR_CC_SHIFT	(30)
#define		LCPR_OP		(1<<28)
#define		LCPR_CXP_MASK	(0x3ff<<16)
#define		LCPR_CXP_SHIFT	(16)
#define		LCPR_CYP_MASK	(0x3ff<<0)
#define		LCPR_CYP_SHIFT	(0)
#define LCDC_LCWHBR(base)  ((base) + 0x10)	//  32bit lcd cursor width/heigh/blink
#define		LCWHBR_BK_EN	(1<<31)
#define		LCWHBR_CW_MASK	(0x1f<<24)
#define		LCWHBR_CW_SHIFT	(24)
#define		LCWHBR_CH_MASK	(0x1f<<16)
#define		LCWHBR_CH_SHIFT (16)
#define		LCWHBR_BD_MASK	(0xff<<0)
#define		LCWHBR_BD_SHIFT	(0)
#define LCDC_LCCMR(base)   ((base) + 0x14)	//  32bit lcd color cursor mapping reg
#define		LCCMR_CUR_COL_R_MASK	(0x3f<<12)
#define		LCCMR_CUR_COL_R_SHIFT	(12)
#define		LCCMR_CUR_COL_G_MASK	(0x3f<<6)
#define		LCCMR_CUR_COL_G_SHIFT	(6)
#define		LCCMR_CUR_COL_B_MASK	(0x3f)
#define		LCCMR_CUR_COL_B_SHIFT	(0)
#define LCDC_LPCR(base)    ((base) + 0x18)	//  32bit lcdc panel config reg
#define		LPCR_TFT		(1<<31)
#define		LPCR_COLOR		(1<<30)
#define		LPCR_PBSIZ_MASK		(3<<28)
#define		LPCR_PBSIZ_SHIFT	(28)
#define         LPCR_PBSIZ_1            (0<<28)
#define         LPCR_PBSIZ_4            (2<<28)
#define         LPCR_PBSIZ_8            (3<<28)
#define		LPCR_BPIX_MASK		(7<<25)
#define		LPCR_BPIX_SHIFT		(25)
#define         LPCR_BPIX_1             (0<<25)
#define         LPCR_BPIX_2             (1<<25)
#define         LPCR_BPIX_4             (2<<25)
#define         LPCR_BPIX_8             (3<<25)
#define         LPCR_BPIX_12            (4<<25)
#define         LPCR_BPIX_16            (5<<25)
#define         LPCR_BPIX_18            (6<<25)
#define		LPCR_PIX_POL		(1<<24)
#define		LPCR_FLMPOL		(1<<23)
#define		LPCR_LPPOL		(1<<22)
#define		LPCR_CLKPOL		(1<<21)
#define		LPCR_OEPOL		(1<<20)
#define		LPCR_SCLKIDLE		(1<<19)
#define		LPCR_END_SEL		(1<<18)
#define		LPCR_SWAP_SEL		(1<<17)
#define		LPCR_REV_VS		(1<<16)
#define		LPCR_ACDSEL		(1<<15)
#define		LPCR_ACD_MASK		(0x7f<<8)
#define		LPCR_ACD_SHIFT		(8)
#define		LPCR_SCLKSEL		(1<<7)
#define		LPCR_SHARP		(1<<6)
#define		LPCR_PCD_MASK		(0x3f<<0)
#define		LPCR_PCD_SHIFT		(0)
#define LCDC_LHCR(base)    ((base) + 0x1C)	//  32bit lcdc horizontal config reg
#define		LHCR_H_WIDTH_MASK	(0x3f << 26)
#define		LHCR_H_WIDTH_SHIFT	(26)
#define		LHCR_H_WAIT_1_MASK	(0xff<<8)
#define		LHCR_H_WAIT_1_SHIFT	(8)
#define		LHCR_H_WAIT_2_MASK	(0xff<<0)
#define		LHCR_H_WAIT_2_SHIFT	(0)
#define LCDC_LVCR(base)    ((base) + 0x20)	//  32bit lcdc vertical config reg
#define		LVCR_V_WIDTH_MASK	(0x3f<<26)
#define		LVCR_V_WIDTH_SHIFT	(26)
#define		LVCR_V_WAIT_1_MASK	(0xff<<8)
#define		LVCR_V_WAIT_1_SHIFT	(8)
#define LCDC_LPOR(base)    ((base) + 0x24)	//  32bit lcdc panning offset reg
#define		LPOR_POS_MASK		(0x1f)
#define		LPOR_POS_SHIFT		(0)
#define LCDC_LSCR(base)    ((base) + 0x28)	//  32bit lcdc sharp config 1 reg
#define		LSCR_PS_RISE_DELAY_MASK		(0x3f<<26)
#define		LSCR_PS_RISE_DELAY_SHIFT	(26)
#define		LSCR_CLS_RISE_DELAY_MASK	(0xff<<16)
#define		LSCR_CLS_RISE_DELAY_SHIFT	(16)
#define		LSCR_REV_TOGGLE_DELAY_MASK	(0xf<<8)
#define		LSCR_REV_TOGGLE_DELAY_SHIFT	(8)
#define		LSCR_GRAY2_MASK			(0xf<<4)
#define		LSCR_GRAY2_SHIFT		(4)
#define		LSCR_GRAY1_MASK			(0xf)
#define		LSCR_GRAY1_SHIFT		(0)
#define LCDC_LPCCR(base)   ((base) + 0x2C)	//  32bit lcdc pwm contrast ctrl reg
#define		LPCCR_CLS_HI_WIDTH_MASK		(0x1ff<<16)
#define		LPCCR_CLS_HI_WIDTH_SHIFT	(16)
#define		LPCCR_LDMSK			(1<<15)
#define		LPCCR_SCR_MASK			(3<<9)
#define		LPCCR_SCR_SHIFT			(9)
#define		LPCCR_CC_EN			(1<<8)
#define		LPCCR_PW_MASK			(0xff<<0)
#define		LPCCR_PW_SHIFT			(0)
#define LCDC_LDCR(base)    ((base) + 0x30)	//  32bit lcdc dma control reg
#define		LDCR_BURST			(1<<31)
#define		LDCR_HM_MASK			(0x1f<<16)
#define		LDCR_HM_SHIFT			(16)
#define		LDCR_TM_MASK			(0x1f<<0)
#define		LDCR_TM_SHIFT			(0)
#define LCDC_LRMCR(base)   ((base) + 0x34)	//  32bit lcdc refresh mode ctrl reg
#define		LRMCR_SELF_REF			(1<<0)
#define LCDC_LICR(base)    ((base) + 0x38)	//  32bit lcdc interrupt config reg
#define		LICR_GW_INT_CON		(1<<4)
#define		LICR_INT_SYN		(1<<2)
#define		LICR_INT_CON		(1<<0)
#define LCDC_LIER(base)    ((base) + 0x3C)	//  32bit lcdc interrupt enable reg
#define		LIER_GW_UDR_ERR_EN	(1<<7)
#define		LIER_GW_ERR_RES_EN	(1<<6)
#define		LIER_GW_EOF_EN		(1<<5)
#define		LIER_GW_BOF_EN		(1<<4)
#define		LIER_UDR_ERR_EN		(1<<3)
#define		LIER_ERR_RES_EN		(1<<2)
#define		LIER_EOF_EN		(1<<1)
#define		LIER_BOF_EN		(1<<0)
#define LCDC_LISR(base)    ((base) + 0x40)	//  32bit lcdc interrupt status reg
#define		LISR_GW_UDR_ERR		(1<<7)
#define		LISR_GW_ERR_RES		(1<<6)
#define		LISR_GW_EOF		(1<<5)
#define		LISR_GW_BOF		(1<<4)
#define		LISR_UDR_ERR		(1<<3)
#define		LISR_ERR_RES		(1<<2)
#define		LISR_EOF		(1<<1)
#define		LISR_BOF		(1<<0)

#define LCDC_LGWSAR(base)  ((base) + 0x50)	//  32bit lcdc graphic win start add
#define LCDC_LGWSR(base)   ((base) + 0x54)	//  32bit lcdc graphic win size reg
#define		LGWSR_GWW_MASK		(0x3f<<20)
#define		LGWSR_GWW_SHIFT		(20)
#define		LGWSR_GWH_MASK		(0x3ff<<0)
#define		LGWSR_GWH_SHIFT		(0)
#define LCDC_LGWVPWR(base) ((base) + 0x58)	//  32bit lcdc graphic win virtual pg
#define		LGWVPWR_GWVPW_MASK	(0x3ff<<0)
#define		LGWVPWR_GWVPW_SHIFT	(0)
#define LCDC_LGWPOR(base)  ((base) + 0x5C)	//  32bit lcdc graphic win pan offset
#define		LGWPOR_GWPO_MASK	(0x1f<<0)
#define		LGWPOR_GWPO_SHIFT	(0)
#define LCDC_LGWPR(base)   ((base) + 0x60)	//  32bit lcdc graphic win positon reg
#define		LGWPR_GWXP_MASK		(0x3ff<<16)
#define		LGWPR_GWXP_SHIFT	(16)
#define		LGWPR_GWYP_MASK		(0x3ff)
#define		LGWPR_GWYP_SHIFT	(0)
#define LCDC_LGWCR(base)   ((base) + 0x64)	//  32bit lcdc graphic win control reg
#define		LGWCR_GWAV_MASK		(0xff<<24)
#define		LGWCR_GWAV_SHIFT	(24)
#define		LGWCR_GWCKE		(1<<23)
#define		LGWCR_GWE		(1<<22)
#define		LGWCR_GW_RVS		(1<<21)
#define		LGWCR_GWCKR_MASK	(0x3f<<12)
#define		LGWCR_GWCKR_SHIFT	(12)
#define		LGWCR_GWCKG_MASK	(0x3f<<6)
#define		LGWCR_GWCKG_SHIFT	(6)
#define		LGWCR_GWCKB_MASK	(0x3f<<0)
#define		LGWCR_GWCKB_SHIFT	(0)
#define LCDC_LGWDCR(base)  ((base) + 0x68)	//  32bit lcdc graphic win DMA control reg
#define		LGWDCR_GWBT		(1<<31)
#define		LGWDCR_GWHM_MASK	(0xf<<16)
#define		LGWDCR_GWHM_SHIFT	(16)
#define		LGWDCR_GWTM_MASK	(0xf<<0)
#define		LGWDCR_GWTM_SHIFT	(0)

#define UPDATE_FIFO_SIZE	(2048)
#define UPDATE_FIFO_MASK	(UPDATE_FIFO_SIZE-1)
#define	UPDATE_FIFO_COUNT(lcdc)	((lcdc)->update_fifo_wp - (lcdc)->update_fifo_rp)
typedef struct ScreenInfo {
	int fb_width;
	int fb_height;
} ScreenInfo;

typedef struct IMXLcdc {
	BusDevice bdev;
	RfbServer *rfbserv;
	int interrupt_posted;
	Clock_t *clk;
	Clock_t *clk_pixel;
	SigNode *irqNode;
	uint32_t lssar;
	uint32_t lsr;
	uint32_t lvpwr;
	uint32_t lcpr;
	uint32_t lcwhbr;
	uint32_t lccmr;
	uint32_t lpcr;
	uint32_t lhcr;
	uint32_t lvcr;
	uint32_t lpor;
	uint32_t lscr;
	uint32_t lpccr;
	uint32_t ldcr;
	uint32_t lrmcr;
	uint32_t licr;
	uint32_t lier;
	uint32_t lisr;
	uint32_t lgwsar;
	uint32_t lgwsr;
	uint32_t lgwvpwr;
	uint32_t lgwpor;
	uint32_t lgwpr;
	uint32_t lgwcr;
	uint32_t lgwdcr;
	/* Currently traced IO-Region */
	uint32_t trace_start;
	uint32_t trace_length;

	CycleTimer updateTimer;
	int page_size;
	uint32_t dirty_addr[UPDATE_FIFO_SIZE];
	uint64_t update_fifo_wp;
	uint64_t update_fifo_rp;
	FbDisplay *display;
} IMXLcdc;

/*
 * -------------------------------------------
 * The event handler called by the Timer 
 * -------------------------------------------
 */
static void
update_display(void *clientData)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	uint32_t addr;
	uint8_t *data;
	FbUpdateRequest fbudrq;
	while (UPDATE_FIFO_COUNT(lcdc) > 0) {
		addr = lcdc->dirty_addr[lcdc->update_fifo_rp & UPDATE_FIFO_MASK];
		addr &= ~(lcdc->page_size - 1);
		Mem_TracePage(addr);
		lcdc->update_fifo_rp++;
		data = Bus_GetHVARead(addr);
		if (data) {
			fbudrq.offset = addr - lcdc->lssar;
			fbudrq.count = lcdc->page_size;
			fbudrq.fbdata = data;
			FbDisplay_UpdateRequest(lcdc->display, &fbudrq);
		}
	}
}

static void
update_fbformat(IMXLcdc * lcdc)
{
	FbFormat fbf;
	uint32_t bpix = lcdc->lpcr & LPCR_BPIX_MASK;
	FbDisplay *display = lcdc->display;
	int endsel = lcdc->lpcr & LPCR_END_SEL;
	if (endsel)
		fprintf(stderr, "LCDC warning: big endian mode not implemented\n");
	switch (bpix) {
	    case LPCR_BPIX_1:
	    case LPCR_BPIX_2:
	    case LPCR_BPIX_4:
	    case LPCR_BPIX_8:
		    fprintf(stderr, "LCDC Warn: unsupported bits per pixel\n");
		    return;

	    case LPCR_BPIX_18:
		    fbf.red_bits = 6;
		    fbf.red_shift = 2;
		    fbf.green_bits = 6;
		    fbf.green_shift = 10;
		    fbf.blue_bits = 6;
		    fbf.blue_shift = 18;
		    fbf.bits_per_pixel = 32;
		    fbf.depth = 18;
		    break;

	    case LPCR_BPIX_12:
		    fbf.red_bits = 4;
		    fbf.green_bits = 4;
		    fbf.blue_bits = 4;
		    fbf.red_shift = 8;
		    fbf.green_shift = 4;
		    fbf.blue_shift = 0;
		    fbf.bits_per_pixel = 16;
		    fbf.depth = 12;
		    break;

	    case LPCR_BPIX_16:
		    fbf.red_bits = 5;
		    fbf.green_bits = 6;
		    fbf.blue_bits = 5;
		    fbf.red_shift = 11;
		    fbf.green_shift = 5;
		    fbf.blue_shift = 0;
		    fbf.bits_per_pixel = 16;
		    fbf.depth = 16;
		    break;

	}
	FbDisplay_SetFbFormat(display, &fbf);
}

static void
lcdc_mem_trace(void *clientData, uint32_t value, uint32_t addr, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	if (UPDATE_FIFO_COUNT(lcdc) == UPDATE_FIFO_SIZE) {
		update_display(lcdc);
	}
	lcdc->dirty_addr[lcdc->update_fifo_wp & UPDATE_FIFO_MASK] = addr;
	lcdc->update_fifo_wp++;
	if (!CycleTimer_IsActive(&lcdc->updateTimer)) {
		CycleTimer_Mod(&lcdc->updateTimer, MillisecondsToCycles(10));
	}
//      fprintf(stderr,"Update at address %08x\n",addr);
}

static void
update_memory_traces(IMXLcdc * lcdc)
{
	uint32_t start, end, length;
	int vpw, height;
	int i;

	vpw = lcdc->lvpwr & LVPWR_VPW_MASK;
	height = ((lcdc->lsr & LSR_YMAX_MASK));
	start = lcdc->lssar;
	length = (vpw << 2) * height;
	end = start + length - 1;
	if ((lcdc->trace_length == length) && (lcdc->trace_start == start)) {
		return;
	}
	if (lcdc->trace_length) {
		IOH_DeleteRegion(lcdc->trace_start, lcdc->trace_length);
		Mem_UntraceRegion(lcdc->trace_start, lcdc->trace_length);
	}
	if (!lcdc->display) {
		return;
	}
	/* Clear all outstanding updates */
	lcdc->update_fifo_rp = lcdc->update_fifo_wp;

	if ((start >= 0xc0000000) && (end <= 0xc7ffffff) && (length > 0)) {
		dbgprintf("updating traced page list %08x to %08x\n", start, end);
		IOH_NewRegion(start, length, NULL, lcdc_mem_trace, 0, lcdc);
		lcdc->trace_length = length;
		lcdc->trace_start = start;
		/* 
		 * Trigger the trace a first time because all pages are dirty 
		 * after address change 
		 */
		for (i = 0; i < length; i += lcdc->page_size) {
			lcdc_mem_trace(lcdc, 0, i + start, 0);
		}
	}
}

static void
update_interrupt(IMXLcdc * lcdc)
{
	if (lcdc->lisr & lcdc->lier) {
		SigNode_Set(lcdc->irqNode, SIG_LOW);
	} else {
		SigNode_Set(lcdc->irqNode, SIG_HIGH);
	}
}

/*
 * --------------------------------------------------------------------
 * LSSAR
 * LCDC Screen Start Adress Register
 * Contains the Screen start address. Bits 0-1 are fixed to 0
 * The picture must be within a 4 MB memory boundary. (Bits
 * 22-31 have a fixed value for an image)
 * --------------------------------------------------------------------
 */
static uint32_t
lssar_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lssar;
}

static void
lssar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lssar = value & 0xfffffffc;
	update_memory_traces(lcdc);
}

/*
 * ---------------------------------------------------------------------------------------
 * LSR
 * 	LCDC Size Register, defines height and width of LCD screen
 *	Bits 20-25 XMAX: Screen width divided by 16. for 1bpp XMAX(20) is ignored
 *	Bits 0-9   YMAX: screen height	
 * ---------------------------------------------------------------------------------------
 */
static uint32_t
lsr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lsr;
}

static void
lsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lsr = value & 0x03f003ff;
	update_memory_traces(lcdc);
	return;
}

/*
 * ---------------------------------------------------------------------------
 * LVPWR
 * 	LCDC Virtual Page Width Register
 * 	Bits 0-9 contain the virtual page width of the LCD panel
 *	used to calculate the startaddress of each displayed line
 * ---------------------------------------------------------------------------
 */
static uint32_t
lvpwr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lvpwr;
}

static void
lvpwr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lvpwr = value & 0x3ff;
	update_memory_traces(lcdc);
}

/* 
 * ---------------------------------------------------------------------------------
 * LCPR:
 * 	LCDC Cursor Position Register 
 *	Bits 30-31 CC: Cursor Control
 *		   00: Transparent, disabled
 *		   01: 1 for non color display, color from cursor color register
 *		   10: reversed
 *		   11: 0 for non color display, AND between bg and cursor for color 
 *	Bit  28:   OP: Arithmetic operation, 0 = disabled, 1 = enabled 
 *	Bits 16-25: CXP: Cursor X position (0..XMAX)
 *	Bits 0-9:   CYP: Cursor Y position
 * ---------------------------------------------------------------------------------
 */
static uint32_t
lcpr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lcpr;
}

static void
lcpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lcpr = value & 0xd7ff03ff;;
	dbgprintf("LCDC: LCPR Cursor position register write has no effect\n");
}

/*
 * ----------------------------------------------------------------------------
 * LCWHBR
 *	LCDC Cursor Width Height and Blink Register
 *	Bit 31		BK_EN:	Blink enable	
 *	Bits 24-28	   CW:  Cursor width in pixels 1-31, 0 = disabled
 *	Bits 0-7	   BD:	Blink Divisor. Divide 32 HZ bei blink divisor 
 * ----------------------------------------------------------------------------
 */
static uint32_t
lcwhbr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lcwhbr;
}

static void
lcwhbr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lcwhbr = value & 0x9f1f00ff;
}

/*
 * -----------------------------------------------------------------------------------
 * LCCMR
 * 	LCDC Color Cursor Mapping Register
 *	Bits 12-17	Red
 *	Bits 6-11	Green
 *	Bits 0-5	Blue
 * -----------------------------------------------------------------------------------
 */
static uint32_t
lccmr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lccmr;
}

static void
lccmr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lccmr = value & 0x0003ffff;
}

/*
 * -----------------------------------------------------------------------------------------
 * LPCR
 *	LCDC Panel Configuration Register
 *	Bit 31    	TFT: 0 = passive display 1 = active, bypass FRC
 *      Bit 30  	COLOR: 0 = monochrome display, 1 = color display
 *	Bit 29-30	PBSIZ: Panel Bus Width
 *			00 = 1 Bit
 *			01 = 4 Bit
 *			11 = 8 Bit
 *	Bits 25-27 	BPIX: 000 = 1bpp, FRC bypassed
 *			      001 = 2 bpp
 *			      010 = 4 bpp
 *			      011 = 8 bpp
 *			      100 = 12 bpp (16 bits of memory used)
 *			      101 = 16 bpp
 *			      110 = 18 bpp (32 bits of memory used)
 *			      111 = reserved
 *	Bit 24		PIXPOL: Pixel polarity 0 = active high, 1 = active low
 * 	Bit 23		FLMPOL: First Line Marker Polarity. 0 = active high, 1 = active low 
 *	Bit 22		LPPOL : Line puls polarity. 0 = active high, 1 = active low
 *	Bit 21		CLKPOL: LCD Shift Clock Polarity. 
 *				0 = negative edge of LSCLK (TFT mode positve)
 *				1 = positive edge of LSCLK (TFT mode negative)
 *	Bit 20		 OEPOL: Output Enable Polarity. 0 = active high, 1 = active high
 *	Bit 19	      SCLKIDLE: enable/disable LSCLK during VSYNC. 0 = disable, 1 = enable
 *	Bit 18	       END_SEL:  Endian Select: 0 = little endian, 1 = big endian
 *	Bit 17	      SWAP_SEL: Swap select in little endian mode: 
 *				0 = 16bpp, 12bpp,
 *				1 = 8,4,2,1bpp
 *		
 *	Bit 16	 	REV_VS: Reverse Vertical Scan (Change SSA Register !!)
 *				0 = normal direction
 *				1 = scan in reverse direction
 *	Bit 15		ACDSEL:	Select clock source used by the alternative crystal
 *				direction counter
 *				0 = Use FRM as clock source for ACD count
 *				1 = Use LP/HSYN as clock source for ACD count
 *	Bit 8-14	   ACD: Toggle ACD every n FLM cycles 
 *	Bit 7	       SCLKSEL: 0 = disable OE and LSCLK in TFT mode when no data output
 *				1 = Always enable LSCLK in TFT mode
 *	Bit 6		 SHARP: Enable signals for Sharp HR-TFTP 240x320 panels
 *	Bit 0-5		   PCD:	Pixelclock divider (divide perclk3)
 *
 * -----------------------------------------------------------------------------------------
 */
static uint32_t
lpcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lpcr;
}

static void
lpcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	int pclkdiv = (value & 0x3f) + 1;
	lcdc->lpcr = value;
	update_fbformat(lcdc);
	Clock_MakeDerived(lcdc->clk_pixel, lcdc->clk, 1, pclkdiv);
}

/*
 * -------------------------------------------------------------------------------
 * LHCR
 * 	LCDC Horizontal Configuration Register
 *	Bits 26-31 H_WIDTH: number of SCLK periods for HSYNC pulse
 *	Bits 8-15  H_WAIT1: number of SCLK periods between end of OE and HSYNC
 *	Bits 0-7   H_WAIT2: number of SCLK periods between HSYNC and next line
 * -------------------------------------------------------------------------------
 */
static uint32_t
lhcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lhcr;
}

static void
lhcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lhcr = value & 0xfc00ffff;
}

/* 
 * ------------------------------------------------------------------------------------------
 * LVCR
 *	LCDC Vertical configuration register
 *	Bits 26-31:	V_WIDTH: number of lines where VSYNC is active
 *	Bits 8-16:	V_WAIT1: number of lines between end of output and start of VSYNC
 *	Bits 0-7:	V_WAIT2: number of lines between VSYNC and first line of frame
 * ------------------------------------------------------------------------------------------
 */
static uint32_t
lvcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lvcr;
}

static void
lvcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lvcr = value & 0xfc00ffff;
	dbgprintf("LCDC: LVCR write has no effect\n");
}

/*
 * ---------------------------------------------------------------------------------
 * LPOR
 *	LCDC Panning Offset Register
 * 	Bits 0-4 POS: Panning offset  
 * ---------------------------------------------------------------------------------
 */
static uint32_t
lpor_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lpor;
}

static void
lpor_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lpor = value & 0x1f;
}

/*
 * --------------------------------------------------------------------------------
 * LSCR
 * 	LCDC Sharp Configuration Register
 * 	Bits 26-31 PS_RISE_DELAY:  0-63 LSCLK periods
 *	Bits 16-23 CLS_RISE_DELAY: 1-255: 2-256 LSCLK periods 0: 10 LSCLK periods
 * --------------------------------------------------------------------------------
 */
static uint32_t
lscr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lscr;
}

static void
lscr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lscr = value & 0xfcff0fff;
}

/*
 * --------------------------------------------------------------------------------
 * LPCCR
 * 	LCDC PWM Contrast Control Register
 *	Bits 16-24: CLS_HI_WIDTH Controls the witdth of CLS signal in units of
 *		    SCLK (CLS_HI_WIDTH+1)
 *	Bits 15  LDMSK: 0 normal LD output 1: LD is always 0 
 *	Bits 9-10  SCR: Source Select for PWM counter.	
 *		   00: Line pulse
 *		   01: Pixel clock
 *		   10: LCDC clock
 *		   11: Reserved
 *	Bit 8	CCN:	Contrast control 0: off 1: on	
 *	Bit 0-7  PW:	Pulse width for PW modulator which controls the contrast
 * --------------------------------------------------------------------------------
 */
static uint32_t
lpccr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lpccr;
}

static void
lpccr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lpccr = value & 0x01ff87ff;
}

/*
 * -----------------------------------------------------------------------------------
 * LDCR:
 *	LCDC graphic window DMA Control Register
 *	Bit 31	BURST:	0 Burst length is dynamic, 1: Burst length is fixed	
 *	Bits 16-20 HM:	DMA high mark
 * 	Bits 0-4   TM:  DMA Trigger mark
 * -----------------------------------------------------------------------------------
 */
static uint32_t
ldcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->ldcr;
}

static void
ldcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->ldcr = value & 0x800f000f;
}

/*
 * ------------------------------------------------------------------------------
 * LRMCR
 *	LCDC Refresh Mode Control Register
 *	Bit 0 SELF_REF: Self refresh 0: disabled 1: enabled
 * ------------------------------------------------------------------------------
 */
static uint32_t
lrmcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lrmcr;
}

static void
lrmcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lrmcr = value & 1;
}

/*
 * -----------------------------------------------------------------------------
 * LICR
 *	LCDC Interrupt Control Register
 * 	Bit 4 GW_INT_CON: Graphic window interrupt condition 
 *			0: int on end of gw
 *			1: int on start of gw
 *	Bit 2 INT_SYN	Interrupt Source
 *	Bit 0 INT CON	Interrupt Condition
 *			00 int on loading last data of frame mem
 *			01 int on loading first data of framem mem
 *			10 int on output last data of frame
 *			11 int on output first data of frame
 *			
 * -----------------------------------------------------------------------------
 */
static uint32_t
licr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->licr;
}

static void
licr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	/* Manual says GW_INT_CON is readonly. I think this is wrong. Please test */
	lcdc->licr = value & 0x15;
	fprintf(stderr, "LCDC: write reg %08x not implemented\n", address);
}

/*
 * ----------------------------------------------------------------------------
 * LIER
 * 	LCDC Interrupt enable register
 *	Bit 7 GW_UDR_ERR_EN: Graphic window underrun error interrupt enable
 *	Bit 6 GW_ERR_RES_EN: Graphic window error response interrupt enable
 *	Bit 5 GW_EOF_EN: Graphic window end of frame interrupt enable	
 *	Bit 4 GW_BOF_EN: Graphic window begin of frame interrupt enable
 *	Background plane:
 *	Bit 3 UDR_ERR_EN: Underrun error interrupt enable
 *	Bit 2 ERR_RES_EN: error response interrupt enable
 *	Bit 1 EOF_EN: end of frame interrupt enable
 *	Bit 0 BOF_EN: begin of frame interupt enable
 * ----------------------------------------------------------------------------
 */
static uint32_t
lier_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lier;
}

static void
lier_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lier = value & 0xff;
	update_interrupt(lcdc);
}

/*
 * ----------------------------------------------------------------------------
 * LISR
 * 	LCDC Interrupt Status Register
 *	Bit 7 GW_UDR_ERR: Graphic window underrun error interrupt
 *	Bit 6 GW_ERR_RES: Graphic window error response interrupt
 *	Bit 5 GW_EOF: Graphic window end of frame interrupt
 *	Bit 4 GW_BOF: Graphic window begin of frame interrupt
 *	Background plane:
 *	Bit 3 UDR_ERR: Underrun error interrupt
 *	Bit 2 ERR_RES: error response interrupt
 *	Bit 1 EOF: end of frame interrupt
 *	Bit 0 BOF: begin of frame interupt
 * ----------------------------------------------------------------------------
 */

static uint32_t
lisr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	uint32_t lier = lcdc->lier;
	lcdc->lier = 0;
	update_interrupt(lcdc);
	return lier;
}

static void
lisr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	fprintf(stderr, "LCDC: LISR is not writable\n");
}

/*
 * ---------------------------------------------------------------------------------------
 * LGWSAR
 * 	LCDC graphic window start address register
 *  	Holds the address of the graphic window picture. Bits 0-1 are fixed to 0.
 *	The image may not cross a 4MB boundary. (A22-31 are not incremented on crossing)
 * ---------------------------------------------------------------------------------------
 */
static uint32_t
lgwsar_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lgwsar;
}

static void
lgwsar_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lgwsar = value & ~3;
	fprintf(stderr, "LCDC: write reg %08x not implemented\n", address);
}

/*
 * --------------------------------------------------------------------------------------
 * LGWSR
 *	LCDC graphic window size register 
 *	Bit 20-25 GWW:	graphic window width / 16. For BW bit 20 is ignored 
 *	Bit 0-9	  GWH:  graphic window width. Value between 1 and GW_YMAX
 *	
 * --------------------------------------------------------------------------------------
 */
static uint32_t
lgwsr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lgwsr;
}

static void
lgwsr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lgwsr = value & 0x03f003ff;
}

/*
 * -----------------------------------------------------------------------------------
 * LGWVPWR
 *	LCDC graphic window virtual page width register
 *	Bits 0-9 GWVPW:	Virtual page width (number of 32-bit words to hold the data 
 *	for one line)
 * -----------------------------------------------------------------------------------
 */
static uint32_t
lgwvpwr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lgwvpwr;
}

static void
lgwvpwr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lgwvpwr = value & 0x3ff;
}

/*
 * ------------------------------------------------------------------------------------
 * LGWPOR
 * 	LCDC graphic window panning offset register
 *	Bits 0-4 Panning of graphic window (0-31). Pan more than32 Bits with LGWSAR
 * ------------------------------------------------------------------------------------
 */
static uint32_t
lgwpor_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lgwpor;
}

static void
lgwpor_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lgwpor = value & 0x1f;
}

/*
 * ----------------------------------------------------------------------------------
 * LGWPR
 *	LCDC graphic window position register
 *	Bits 16-25	GWXP: x-position
 *	Bits 0-9	GWYP: y-position
 * ----------------------------------------------------------------------------------
 */
static uint32_t
lgwpr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lgwpr;
}

static void
lgwpr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lgwpr = value & 0x03ff03ff;
}

/*
 * ----------------------------------------------------------------------------------
 * LGWCR
 *	LCDC Graphic Window Control Register
 *	Bits 24-31 GWAV: Graphic window Alpha Value	
 *	Bit 23	  GWCKE: Graphic window color keying enable
 *	Bit 22	    GWE: Graphic window enable
 *	Bit 21   GW_RVS: Graphic window reverse vertical scan
 *	Bits 12-17 GWCKR: Graphic window color keying red component
 *	Bits 6-11  GWCKG: Graphic window color keying green component
 *	Bits 0-5   GWCKB: Graphic window color keying blue component
 * ----------------------------------------------------------------------------------
 */

static uint32_t
lgwcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lgwcr;
}

static void
lgwcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lgwcr = value & 0xffe3ffff;
	fprintf(stderr, "LCDC: write reg %08x not implemented\n", address);
}

/*
 * ----------------------------------------------------------------------------------
 * LGWDCR
 * 	LCDC graphic window dma control register
 *	Bit 31 GWBT: graphic window dma burst type
 *	Bits 16-20 GWHM:  Graphic window DMA high mark 
 *      Bits 0-4   GWTM:  Graphic window DMA low mark
 * ----------------------------------------------------------------------------------
 */
static uint32_t
lgwdcr_read(void *clientData, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	return lcdc->lgwdcr;
}

static void
lgwdcr_write(void *clientData, uint32_t value, uint32_t address, int rqlen)
{
	IMXLcdc *lcdc = (IMXLcdc *) clientData;
	lcdc->lgwdcr = value & 0x801f001f;
}

static void
IMXLcdc_UnMap(void *owner, uint32_t base, uint32_t mapsize)
{
	IOH_Delete32(LCDC_LSSAR(base));
	IOH_Delete32(LCDC_LSR(base));
	IOH_Delete32(LCDC_LVPWR(base));
	IOH_Delete32(LCDC_LCPR(base));
	IOH_Delete32(LCDC_LCWHBR(base));
	IOH_Delete32(LCDC_LCCMR(base));
	IOH_Delete32(LCDC_LPCR(base));
	IOH_Delete32(LCDC_LHCR(base));
	IOH_Delete32(LCDC_LVCR(base));
	IOH_Delete32(LCDC_LPOR(base));
	IOH_Delete32(LCDC_LSCR(base));
	IOH_Delete32(LCDC_LPCCR(base));
	IOH_Delete32(LCDC_LDCR(base));
	IOH_Delete32(LCDC_LRMCR(base));
	IOH_Delete32(LCDC_LICR(base));
	IOH_Delete32(LCDC_LIER(base));
	IOH_Delete32(LCDC_LISR(base));
	IOH_Delete32(LCDC_LGWSAR(base));
	IOH_Delete32(LCDC_LGWSR(base));
	IOH_Delete32(LCDC_LGWVPWR(base));
	IOH_Delete32(LCDC_LGWPOR(base));
	IOH_Delete32(LCDC_LGWPR(base));
	IOH_Delete32(LCDC_LGWCR(base));
	IOH_Delete32(LCDC_LGWDCR(base));
}

static void
IMXLcdc_Map(void *owner, uint32_t base, uint32_t mask, uint32_t flags)
{
	IMXLcdc *lcd = (IMXLcdc *) owner;
	IOH_New32(LCDC_LSSAR(base), lssar_read, lssar_write, lcd);
	IOH_New32(LCDC_LSR(base), lsr_read, lsr_write, lcd);
	IOH_New32(LCDC_LVPWR(base), lvpwr_read, lvpwr_write, lcd);
	IOH_New32(LCDC_LCPR(base), lcpr_read, lcpr_write, lcd);
	IOH_New32(LCDC_LCWHBR(base), lcwhbr_read, lcwhbr_write, lcd);
	IOH_New32(LCDC_LCCMR(base), lccmr_read, lccmr_write, lcd);
	IOH_New32(LCDC_LPCR(base), lpcr_read, lpcr_write, lcd);
	IOH_New32(LCDC_LHCR(base), lhcr_read, lhcr_write, lcd);
	IOH_New32(LCDC_LVCR(base), lvcr_read, lvcr_write, lcd);
	IOH_New32(LCDC_LPOR(base), lpor_read, lpor_write, lcd);
	IOH_New32(LCDC_LSCR(base), lscr_read, lscr_write, lcd);
	IOH_New32(LCDC_LPCCR(base), lpccr_read, lpccr_write, lcd);
	IOH_New32(LCDC_LDCR(base), ldcr_read, ldcr_write, lcd);
	IOH_New32(LCDC_LRMCR(base), lrmcr_read, lrmcr_write, lcd);
	IOH_New32(LCDC_LICR(base), licr_read, licr_write, lcd);
	IOH_New32(LCDC_LIER(base), lier_read, lier_write, lcd);
	IOH_New32(LCDC_LISR(base), lisr_read, lisr_write, lcd);
	IOH_New32(LCDC_LGWSAR(base), lgwsar_read, lgwsar_write, lcd);
	IOH_New32(LCDC_LGWSR(base), lgwsr_read, lgwsr_write, lcd);
	IOH_New32(LCDC_LGWVPWR(base), lgwvpwr_read, lgwvpwr_write, lcd);
	IOH_New32(LCDC_LGWPOR(base), lgwpor_read, lgwpor_write, lcd);
	IOH_New32(LCDC_LGWPR(base), lgwpr_read, lgwpr_write, lcd);
	IOH_New32(LCDC_LGWCR(base), lgwcr_read, lgwcr_write, lcd);
	IOH_New32(LCDC_LGWDCR(base), lgwdcr_read, lgwdcr_write, lcd);

}

BusDevice *
IMX21_LcdcNew(const char *name, FbDisplay * display)
{
	IMXLcdc *lcdc = sg_new(IMXLcdc);
	if (!display) {
		fprintf(stderr, "i.MX21 LCD controller requires a valid display\n");
		exit(1);
	}
	lcdc->irqNode = SigNode_New("%s.irq", name);
	if (!lcdc->irqNode) {
		fprintf(stderr, "LCDC: creation of irq node failed\n");
		exit(1);
	}
	lcdc->display = display;
	lcdc->page_size = Mem_SmallPageSize();
	lcdc->ldcr = 0x80080004;
	CycleTimer_Init(&lcdc->updateTimer, update_display, lcdc);
	lcdc->bdev.first_mapping = NULL;
	lcdc->bdev.Map = IMXLcdc_Map;
	lcdc->bdev.UnMap = IMXLcdc_UnMap;
	lcdc->bdev.owner = lcdc;
	lcdc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	lcdc->clk = Clock_New("%s.clk", name);
	lcdc->clk_pixel = Clock_New("%s.pixelclk", name);
	fprintf(stderr, "i.MX21 LCD-Controller \"%s\" created\n", name);
	return &lcdc->bdev;
}
