/*
 ***********************************************************************************************
 *
 * Emulation of the AT91 LCD Controller (LCDC) 
 *
 *  State: not implemented 
 *
 * Copyright 2012 Jochen Karrer. All rights reserved.
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

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "signode.h"
#include "bus.h"
#include "sgstring.h"
#include "at91_lcdc.h"
#include "cycletimer.h"
#include "clock.h"
#include "fbdisplay.h"

#define LCDC_DMABADDR1(base)	((base) + 0x00)
#define LCDC_DMABADDR2(base)	((base) + 0x04)
#define LCDC_DMAFRMPT1(base)	((base) + 0x08)
#define LCDC_DMAFRMPT2(base)	((base) + 0x0c)
#define LCDC_DMAFRMADD1(base)	((base) + 0x10)
#define LCDC_DMAFRMADD2(base)	((base) + 0x14)
#define LCDC_DMAFRMCFG(base)	((base) + 0x18)
#define	LCDC_DMACON(base)	((base) + 0x1c)
#define LCDC_DMA2DCFG(base)	((base) + 0x20)
#define LCDC_LCDCON1(base)	((base) + 0x800)
#define LCDC_LCDCON2(base)	((base) + 0x804)
#define LCDC_LCDTIM1(base)	((base) + 0x808)
#define LCDC_LCDTIM2(base)	((base) + 0x80c)
#define LCDC_LCDFRMCFG(base)	((base) + 0x810)
#define LCDC_LCDFIFO(base)	((base) + 0x814)
#define LCDC_DP1_2(base)	((base) + 0x81c)
#define LCDC_DP4_7(base)	((base) + 0x820)
#define LCDC_DP3_5(base)	((base) + 0x824)
#define LCDC_DP2_3(base)	((base) + 0x828)
#define LCDC_DP5_7(base)	((base) + 0x82c)
#define LCDC_DP3_4(base)	((base) + 0x830)
#define LCDC_DP4_5(base)	((base) + 0x834)
#define LCDC_DP6_7(base)	((base) + 0x838)
#define LCDC_PWRCON(base)	((base) + 0x83c)
#define LCDC_CONTRAST_CTR(base)	((base) + 0x840)
#define LCDC_CONTRAST_VAL(base)	((base) + 0x844)
#define LCDC_LCD_IER(base)	((base) + 0x848)
#define LCDC_LCD_IDR(base)	((base) + 0x84c)
#define LCDC_LCD_IMR(base)	((base) + 0x850)
#define LCDC_LCD_ISR(base)	((base) + 0x854)
#define LCDC_LCD_ICR(base)	((base) + 0x858)
#define LCDC_LCD_ITR(base)	((base) + 0x860)
#define LCDC_LCD_IRR(base)	((base) + 0x864)
#define LCDC_LUT_ENTRY(base,x)	((base) + 0xc00 + ((x) << 2))

#define UPDATE_FIFO_SIZE        (512)
#define UPDATE_FIFO_MASK        (UPDATE_FIFO_SIZE-1)
#define UPDATE_FIFO_RP(lcdc)	((lcdc)->updateFifoRp & UPDATE_FIFO_MASK)
#define UPDATE_FIFO_WP(lcdc)	((lcdc)->updateFifoWp & UPDATE_FIFO_MASK)
#define UPDATE_FIFO_COUNT(lcdc) ((lcdc)->updateFifoWp - (lcdc)->updateFifoRp)

typedef struct AT91Lcdc {
	BusDevice bdev;
	uint32_t regDMABADDR1;
	uint32_t regDMABADDR2;
	uint32_t regDMAFRMPT1;
	uint32_t regDMAFRMPT2;
	uint32_t regDMAFRMADD1;
	uint32_t regDMAFRMADD2;
	uint32_t regDMAFRMCFG;
	uint32_t regDMACON;
	uint32_t regDMA2DCFG;
	uint32_t regLCDCON1;
	uint32_t regLCDCON2;
	uint32_t regLCDTIM1;
	uint32_t regLCDTIM2;
	uint32_t regLCDFRMCFG;
	uint32_t regLCDFIFO;
	uint32_t regDP1_2;
	uint32_t regDP4_7;
	uint32_t regDP3_5;
	uint32_t regDP2_3;
	uint32_t regDP5_7;
	uint32_t regDP3_4;
	uint32_t regDP4_5;
	uint32_t regDP6_7;
	uint32_t regPWRCON;
	uint32_t regCONTRAST_CTR;
	uint32_t regCONTRAST_VAL;
	uint32_t regLCD_IMR;
	uint32_t regLCD_ISR;
	uint32_t regLCD_ICR;
	uint32_t regLCD_ITR;
	uint32_t regLCD_IRR;
	uint32_t regLUT[256];

	uint32_t pageSize;
	uint32_t updateFifo[UPDATE_FIFO_SIZE];
        uint64_t updateFifoWp;
        uint64_t updateFifoRp;
        FbDisplay *display;
	CycleTimer updateTimer;
	uint32_t traceStart;
	uint32_t traceLength;
} AT91Lcdc;

/*
 ******************************************************************
 * This event handler is called by the Timer about 10ms after
 * a page was modified or if the update fifo is full.
 * It reinstalls the Memory trace
 ******************************************************************
 */
static void
update_display(void *clientData)
{
        AT91Lcdc *lcdc = (AT91Lcdc*)clientData;
        uint32_t addr;
        uint8_t *data;
        FbUpdateRequest fbudrq;
        while(UPDATE_FIFO_COUNT(lcdc) > 0) {
                addr = lcdc->updateFifo[UPDATE_FIFO_RP(lcdc)];
                addr &= ~(lcdc->pageSize - 1);
                Mem_TracePage(addr);
                lcdc->updateFifoRp++;
                data = Bus_GetHVARead(addr);
                if(data) {
			fbudrq.offset = addr - lcdc->regDMABADDR1;
                        fbudrq.count = lcdc->pageSize;
                        fbudrq.fbdata = data;
			//fprintf(stderr,"UDRQ, offs %08x\n",fbudrq.offset);
                        FbDisplay_UpdateRequest(lcdc->display,&fbudrq);
                }
        }
}


static void
lcdc_mem_trace(void *clientData, uint32_t value,uint32_t addr, int rqlen)
{
        AT91Lcdc *lcdc = (AT91Lcdc *) clientData;
        if(UPDATE_FIFO_COUNT(lcdc) == UPDATE_FIFO_SIZE) {
                update_display(lcdc);
        }
        lcdc->updateFifo[UPDATE_FIFO_WP(lcdc)] = addr;
        lcdc->updateFifoWp++;
        if(!CycleTimer_IsActive(&lcdc->updateTimer)) {
                CycleTimer_Mod(&lcdc->updateTimer,MillisecondsToCycles(15));
        }
}

static uint32_t 
get_pixelsize(AT91Lcdc *lcdc) 
{
	int ps = (lcdc->regLCDCON2 >> 5) & 7;
	unsigned int pixelsize = 0;
	switch(ps) {
		case 0:
			pixelsize = 1;
			break;
		case 1:
			pixelsize = 2;
			break;
		case 2:
			pixelsize = 4;
			break;
		case 3: 
			pixelsize = 8;
			break;
		case 4:
			pixelsize = 16;
			break;
		case 5:
			pixelsize = 24;
			break;
		case 6: 
			pixelsize = 32;
			break;
		case 7:
			break;
	}
	return pixelsize;
}
static void
update_memory_traces(AT91Lcdc *lcdc)
{
	unsigned int height, width;	
	unsigned int bipp,length;
	uint32_t startAddr;
	uint32_t i;
	width = (lcdc->regLCDFRMCFG >> 21 & 0x7ff) + 1;
	height = (lcdc->regLCDFRMCFG & 0x7ff) + 1; 
	startAddr = lcdc->regDMABADDR1;
	bipp = get_pixelsize(lcdc); 
	length = (width * height * bipp + 7) / 8;
	if((lcdc->traceLength == length) && (lcdc->traceStart == startAddr)) {
		return;
	}
	if(lcdc->traceLength) {
                IOH_DeleteRegion(lcdc->traceStart,lcdc->traceLength) ;
                Mem_UntraceRegion(lcdc->traceStart,lcdc->traceLength);
		lcdc->traceLength = lcdc->traceStart = 0;
        }
	fprintf(stderr,"start 0x%08x %ux%u, length %u\n",startAddr,width,height,length);
	if((height == 1) || (width == 1) || (startAddr < 0x20000000) || ((startAddr + length) > 0x30000000)) {
		return;
	}
	if(!lcdc->display) {
		return;
	}	
	fprintf(stderr,"Tracing memory at %08x, len %u\n",startAddr,length);
	/* Clear all outstanding updates */
	lcdc->updateFifoRp = lcdc->updateFifoWp;
	IOH_NewRegion(startAddr,length,NULL,lcdc_mem_trace,0,lcdc);
	lcdc->traceLength = length;
	lcdc->traceStart = startAddr;
	/* 
	 * Trigger the trace a first time because all pages are dirty 
	 * after address change 
	 */
	for(i = 0;i < length;i += lcdc->pageSize) {
		lcdc_mem_trace(lcdc,0,i + startAddr,0);
	}
}

/**
 ************************************************************************
 * \fn static void update_fbformat(AT91Lcdc *lcdc); 
 * Warning: Incomplete, only a default format is implemented. 
 ************************************************************************
 */
static void
update_fbformat(AT91Lcdc *lcdc) {
	FbFormat fbf;
	fbf.red_bits = 5;
	fbf.green_bits = 6;
	fbf.blue_bits = 5;
	fbf.red_shift = 11;
	fbf.green_shift = 5;
	fbf.blue_shift = 0;
	fbf.bits_per_pixel = 16;
	fbf.depth = 16;
	FbDisplay_SetFbFormat(lcdc->display,&fbf);
}

/**
 *******************************************************************************************
 * Base Address for the upper panel in dual scan mode  or for the complete frame in single
 * scan mode.
 *******************************************************************************************
 */
static uint32_t
dmabaddr1_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	return lcdc->regDMABADDR1;
}

static void
dmabaddr1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	lcdc->regDMABADDR1 = value;
	update_memory_traces(lcdc);
}

/**
 ******************************************************************************
 * DMA base address for the lower panel in dual scan mode.
 ******************************************************************************
 */
static uint32_t
dmabaddr2_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	return lcdc->regDMABADDR2;
}

static void
dmabaddr2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	lcdc->regDMABADDR2 = value;
}

/**
 ***************************************************************************
 * DMA frame pointer for upper panel or for single scan mode.
 * (Number of words to the end of frame);
 ***************************************************************************
 */
static uint32_t
dmafrmpt1_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	return lcdc->regDMAFRMPT1;
}

static void
dmafrmpt1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC DMA Framepointer is readonly\n");
}

/**
 ***************************************************************************
 * DMA frame pointer for lower half of panel
 ***************************************************************************
 */
static uint32_t
dmafrmpt2_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	return lcdc->regDMAFRMPT2;
}

static void
dmafrmpt2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC DMA Framepointer is readonly\n");
}

/**
 **********************************************************
 * Current DMA address 
 **********************************************************
 */
static uint32_t
dmafrmadd1_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	return lcdc->regDMAFRMADD1;
}

static void
dmafrmadd1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC DMA Frame address is not writable\n");
}

/**
 ****************************************************************
 * Current DMA address
 ****************************************************************
 */
static uint32_t
dmafrmadd2_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	return lcdc->regDMAFRMADD2;
}

static void
dmafrmadd2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC DMA Frame address is not writable\n");
}

/**
 ***************************************************************************
 * dmafrmcfg
 * Contains the frame size and the burst len for DMA transfer.
 ***************************************************************************
 */
static uint32_t
dmafrmcfg_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dmafrmcfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

/**
 ******************************************************************************
 * DMACON register.
 * Bit 0: DMAEN
 * Bit 1: DMARST 
 * Bit 2: DMABUSY
 * Bit 3: DMAUPDT 
 * Bit 4: DMA2DEN DMA 2D Addressing enable
 ******************************************************************************
 */
static uint32_t
dmacon_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dmacon_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

/**
 **************************************************************************************
 * DMA2DCFG DMA 2D Addressing configuration register.
 * Bit 0-16 ADDRINC: Number of bytes between lines in 2D mode
 * Bit 24 - 28 Pixeloffset for the first pixel in the line in a 32 Bit word
 **************************************************************************************
 */
static uint32_t
dma2dcfg_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
        return lcdc->regDMA2DCFG;
}

static void
dma2dcfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
        lcdc->regDMA2DCFG = value & 0x1f00ffff;
}

/**
 *********************************************************************
 * LCDCON1
 * Bit 0: BYPASS The LCDDOTCK divider is bypassed (division by one)
 * Bit 12 - 20: CLKVAL, pixel_clock = system_clock / (2 * CLKVAL + 2)
 * Bit 21 - 31: LINECNT 11 Bit Line counter.
 *********************************************************************
 */
static uint32_t
lcdcon1_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	// update_linecnt();
        return lcdc->regLCDCON1;
}

static void
lcdcon1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	lcdc->regLCDCON1 = (value & 0x001FF001) | (lcdc->regLCDCON1 & 0xffe00000);
}

/**
 **************************************************************************************
 * LCDCON2
 * Bit 0 - 1: DISTYPE 00 = STN Mono 01 = STN Color 02 = TFT 03 Reserved
 * Bit 2: SCANMOD 0 = Single Scan, 1 = Dual Scan
 * Bit 3 - 4:  IFWIDTH (STN) 
 * Bit 5 - 7: Pixelsize
 * Bit 8: INVVD LCDD polarity 0 = normal, 1 = inverted
 * Bit 9: INVFRAME: Invert LCD vsync 0 = normal, 1 = inverted
 * Bit 10: INVLINE: Invert LCD hsync 0 = normal, 1 = inverted
 * Bit 11: INVCLK: Invert LCD clock: 0 = LCDD fetched at falling edge 1 = rising
 * Bit 12: INVDVAL: LCDDEN polarity 0 = active high (normal) 1 = active low
 * Bit 15: CLKMOD: 0 = clk only active during display period, 1 = always active
 * Bit 30 - 31: MEMOR Memory Ordering 00 Big Endian 10 Little, 11: Wince format
 **************************************************************************************
 */ 
static uint32_t
lcdcon2_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
        return lcdc->regLCDCON2;
}

static void
lcdcon2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
        lcdc->regLCDCON2 = value & 0xc0009fff;
}

/**
 **********************************************************************************************
 * LCDTIM1
 * Bit 0 - 7: VFP Vertical front porch (TFT) number of idle lines at the end of the frame
 * Bit 8 - 15: VBP Vertical back porch. The number of idle lines at the beginning of the frame 
 * Bit 16 - 21: VPW Vertical sync pulse with (VPW + 1)
 * Bit 24 - 27: VHDLY Vertical to horizontal delay
 **********************************************************************************************
 */
static uint32_t
lcdtim1_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
lcdtim1_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

/**
 ********************************************************************************************************
 * LCDTIM2 
 * Bit 0 - 7: HBP Horizontal Back Porch number of idle clock cycles at the beginning of the line (HBP +1)
 * Bit 8 - 13: HPW Horizontal sync pulsw width (HPW + 1)
 * Bit 21 - 31: HFP Horizontal front porch number of idle clock cycles at end of line (HFP + 1) 
 ********************************************************************************************************
 */
static uint32_t
lcdtim2_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
lcdtim2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

/**
 *************************************************************************************************
 * LCDFRMCFG
 * Bit 0 - 10 LINEVAL Vertical size of LCD module or panel in pixels minus 1 
 * Bit 21 - 31 LINESIZE Horizontal size of LCD module in pixels minus 1
 *************************************************************************************************
 */
static uint32_t
lcdfrmcfg_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
        return lcdc->regLCDFRMCFG;
}

static void
lcdfrmcfg_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	lcdc->regLCDFRMCFG = value & 0xffe007ff;
	update_memory_traces(lcdc);
}

/**
 **************************************************************
 * LCD Fifo Threshold register
 **************************************************************
 */
static uint32_t
lcdfifo_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
lcdfifo_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
dp1_2_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dp1_2_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
dp4_7_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dp4_7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
dp3_5_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dp3_5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
dp2_3_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dp2_3_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
dp5_7_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dp5_7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
dp3_4_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dp3_4_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
dp4_5_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dp4_5_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
dp6_7_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
dp6_7_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

/*
 *********************************************************************************
 * Power control
 * Bit 0: LCD_PWR 0 = lcd_pwr pin = low other LCD pins also
 * Bit 1 - 7: Guard Time Delay in frame periods between applying control signals
 * 	      and setting LCD_PWR high and opposite way. 
 * Bit 31:  LCD busy
 *********************************************************************************
 */
static uint32_t
pwrcon_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
pwrcon_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
contrast_ctr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
contrast_ctr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
contrast_val_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
contrast_val_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
ier_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
ier_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
idr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
idr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
imr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
imr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
isr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
isr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
icr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
icr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
itr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
itr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
irr_read(void *clientData,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
        return 0;
}

static void
irr_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	fprintf(stderr,"LCDC_%s not implemented\n",__func__);
}

static uint32_t
lut_entry_read(void *clientData,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	uint32_t index = (address >> 2) & 0xff;
	uint32_t value;
	value = lcdc->regLUT[index];
        return value;
}

static void
lut_entry_write(void *clientData,uint32_t value,uint32_t address,int rqlen)
{
	AT91Lcdc *lcdc = clientData;
	uint32_t index = (address >> 2) & 0xff;
	lcdc->regLUT[index] = value;
}

static void
AT91Lcdc_Map(void *owner,uint32_t base,uint32_t mask,uint32_t flags)
{
        AT91Lcdc *lcdc = (AT91Lcdc*) owner;
	unsigned int i;
        IOH_New32(LCDC_DMABADDR1(base),dmabaddr1_read,dmabaddr1_write,lcdc);
	IOH_New32(LCDC_DMABADDR2(base),dmabaddr2_read,dmabaddr2_write,lcdc);
	IOH_New32(LCDC_DMAFRMPT1(base),dmafrmpt1_read,dmafrmpt1_write,lcdc);
	IOH_New32(LCDC_DMAFRMPT2(base),dmafrmpt2_read,dmafrmpt2_write,lcdc);
	IOH_New32(LCDC_DMAFRMADD1(base),dmafrmadd1_read,dmafrmadd1_write,lcdc);
	IOH_New32(LCDC_DMAFRMADD2(base),dmafrmadd2_read,dmafrmadd2_write,lcdc);
	IOH_New32(LCDC_DMAFRMCFG(base),dmafrmcfg_read,dmafrmcfg_write,lcdc);
	IOH_New32(LCDC_DMACON(base),dmacon_read,dmacon_write,lcdc);
	IOH_New32(LCDC_DMA2DCFG(base),dma2dcfg_read,dma2dcfg_write,lcdc);
	IOH_New32(LCDC_LCDCON1(base),lcdcon1_read,lcdcon1_write,lcdc);
	IOH_New32(LCDC_LCDCON2(base),lcdcon2_read,lcdcon2_write,lcdc);
	IOH_New32(LCDC_LCDTIM1(base),lcdtim1_read,lcdtim1_write,lcdc);
	IOH_New32(LCDC_LCDTIM2(base),lcdtim2_read,lcdtim2_write,lcdc);
	IOH_New32(LCDC_LCDFRMCFG(base),lcdfrmcfg_read,lcdfrmcfg_write,lcdc);
	IOH_New32(LCDC_LCDFIFO(base),lcdfifo_read,lcdfifo_write,lcdc);
	IOH_New32(LCDC_DP1_2(base),dp1_2_read,dp1_2_write,lcdc);
	IOH_New32(LCDC_DP4_7(base),dp4_7_read,dp4_7_write,lcdc);
	IOH_New32(LCDC_DP3_5(base),dp3_5_read,dp3_5_write,lcdc);
	IOH_New32(LCDC_DP2_3(base),dp2_3_read,dp2_3_write,lcdc);
	IOH_New32(LCDC_DP5_7(base),dp5_7_read,dp5_7_write,lcdc);
	IOH_New32(LCDC_DP3_4(base),dp3_4_read,dp3_4_write,lcdc);
	IOH_New32(LCDC_DP4_5(base),dp4_5_read,dp4_5_write,lcdc);
	IOH_New32(LCDC_DP6_7(base),dp6_7_read,dp6_7_write,lcdc);
	IOH_New32(LCDC_PWRCON(base),pwrcon_read,pwrcon_write,lcdc);
	IOH_New32(LCDC_CONTRAST_CTR(base),contrast_ctr_read,contrast_ctr_write,lcdc);
	IOH_New32(LCDC_CONTRAST_VAL(base),contrast_val_read,contrast_val_write,lcdc);
	IOH_New32(LCDC_LCD_IER(base),ier_read,ier_write,lcdc);
	IOH_New32(LCDC_LCD_IDR(base),idr_read,idr_write,lcdc);
	IOH_New32(LCDC_LCD_IMR(base),imr_read,imr_write,lcdc);
	IOH_New32(LCDC_LCD_ISR(base),isr_read,isr_write,lcdc);
	IOH_New32(LCDC_LCD_ICR(base),icr_read,icr_write,lcdc);
	IOH_New32(LCDC_LCD_ITR(base),itr_read,itr_write,lcdc);
	IOH_New32(LCDC_LCD_IRR(base),irr_read,irr_write,lcdc);
	for(i = 0; i < 256; i++) {
		IOH_New32(LCDC_LUT_ENTRY(base,i),lut_entry_read,lut_entry_write,lcdc);
	}
}

static void
AT91Lcdc_UnMap(void *owner,uint32_t base,uint32_t mask)
{
	unsigned int i;
        IOH_Delete32(LCDC_DMABADDR1(base));
	IOH_Delete32(LCDC_DMABADDR2(base));
	IOH_Delete32(LCDC_DMAFRMPT1(base));
	IOH_Delete32(LCDC_DMAFRMPT2(base));
	IOH_Delete32(LCDC_DMAFRMADD1(base));
	IOH_Delete32(LCDC_DMAFRMADD2(base));
	IOH_Delete32(LCDC_DMAFRMCFG(base));
	IOH_Delete32(LCDC_DMACON(base));
	IOH_Delete32(LCDC_DMA2DCFG(base));
	IOH_Delete32(LCDC_LCDCON1(base));
	IOH_Delete32(LCDC_LCDCON2(base));
	IOH_Delete32(LCDC_LCDTIM1(base));
	IOH_Delete32(LCDC_LCDTIM2(base));
	IOH_Delete32(LCDC_LCDFRMCFG(base));
	IOH_Delete32(LCDC_LCDFIFO(base));
	IOH_Delete32(LCDC_DP1_2(base));
	IOH_Delete32(LCDC_DP4_7(base));
	IOH_Delete32(LCDC_DP3_5(base));
	IOH_Delete32(LCDC_DP2_3(base));
	IOH_Delete32(LCDC_DP5_7(base));
	IOH_Delete32(LCDC_DP3_4(base));
	IOH_Delete32(LCDC_DP4_5(base));
	IOH_Delete32(LCDC_DP6_7(base));
	IOH_Delete32(LCDC_PWRCON(base));
	IOH_Delete32(LCDC_CONTRAST_CTR(base));
	IOH_Delete32(LCDC_CONTRAST_VAL(base));
	IOH_Delete32(LCDC_LCD_IER(base));
	IOH_Delete32(LCDC_LCD_IDR(base));
	IOH_Delete32(LCDC_LCD_IMR(base));
	IOH_Delete32(LCDC_LCD_ISR(base));
	IOH_Delete32(LCDC_LCD_ICR(base));
	IOH_Delete32(LCDC_LCD_ITR(base));
	IOH_Delete32(LCDC_LCD_IRR(base));
	for(i = 0; i < 256; i++) {
		IOH_Delete32(LCDC_LUT_ENTRY(base,i));
	}
}


BusDevice *
AT91Lcdc_New(const char *name,FbDisplay *display)
{
	AT91Lcdc *lcdc = sg_new(AT91Lcdc);
	lcdc->bdev.first_mapping = NULL;
        lcdc->bdev.Map = AT91Lcdc_Map;
        lcdc->bdev.UnMap = AT91Lcdc_UnMap;
        lcdc->bdev.owner = lcdc;
        lcdc->bdev.hw_flags = MEM_FLAG_WRITABLE | MEM_FLAG_READABLE;
	lcdc->display = display;
	lcdc->pageSize = Mem_SmallPageSize();
	lcdc->traceLength = lcdc->traceStart = 0;
	CycleTimer_Init(&lcdc->updateTimer,update_display,lcdc);
	update_fbformat(lcdc); 
	fprintf(stderr,"AT91 LCD controller \"%s\" created\n",name);
	return &lcdc->bdev;
}
