/*
 ******************************************************************************
 * Emulation of LCD Data Modul DM128064 Graphics display.
 * Uses a Solomon SSD1815 Chip.
 * Seems to be similar to Sitronix ST7565r
 *
 * state: Working, reverse and scan direction inversion missing 
 *
 * Copyright 2010 Jochen Karrer. All rights reserved.
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

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include "signode.h"
#include "sgstring.h"
#include "cycletimer.h"
#include "fbdisplay.h"
#include "configfile.h"

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

#define CMD_LOWCOLADR(col)	(0x00 | ((col) & 0xf)) 
#define CMD_HICOLADR(col)	(0x10 | (((col) >> 4) & 0xf))
#define CMD_INTRESRATIO(x)	(0x20 | ((x) & 7))
#define CMD_POWCONTROL(x)	(0x28 | ((x) & 7))
#define CMD_STARTLINE(x)	(0x40 | ((x) & 0x3f))
#define CMD_CONTRAST_1		(0x81)
#define CMD_CONTRAST_2(kon)	((kon) & 0x3f)
#define CMD_SEGREMAP(x)		(0xa0 | ((x) & 1))
#define CMD_LCDBIAS_1_9(x)	(0xa2) 
#define CMD_LCDBIAS_1_7(x)	(0xa3) 
#define CMD_ENTDISPNOTON	(0xa4)
#define CMD_ENTDISPON		(0xa5)
#define CMD_NONREVERSE		(0xa6)
#define CMD_REVERSE		(0xa7)
#define CMD_DISPOFF		(0xae)
#define CMD_DISPON		(0xaf)
#define CMD_PAGEADR(pa)		(0xb0 | ((pa) & 0xf))
#define CMD_COMSCANDIR_NORM	(0xc0) 
#define CMD_COMSCANDIR_REMAP	(0xc8) 
#define CMD_READMODWRITE	(0xe0)
#define CMD_SWRESET		(0xe2)
#define CMD_ENDREADMODWRITE	(0xee)
#define CMD_INDICATOR_OFF	(0xac)
#define CMD_INDICATOR_ON_1	(0xad)
#define CMD_INDICATOR_ON_2(m)	((m) & 3)
#define CMD_NOP			(0xe3)
#define CMD_POWERSAVE		/* ???? */
#define ECMD_SETMUXRATIO_1	(0xa8) /* DM Manual is wrong here */
#define ECMD_SETMUXRATIO_2(r)	((r) & 0x3f)
#define ECMD_SETBIASTC_1	(0xa9)
#define ECMD_SETBIASTC_2(f,tc,b)	(((f) << 5)  | ((tc << 2)) | (b))
#define ECMD_BIAS_1_4		(0xab)
#define ECMD_BIAS_NORM		(0xaa)
#define ECMD_FRAME_PHASES_1	(0xd2)
#define ECMD_FRAME_PHASES_2(p)	(0x2 | ((p) << 5))
#define ECMD_SETDISPOFFS_1	(0xd3)
#define ECMD_SETDISPOFFS_2(ofs)	((ofs) & 0x3f)

#define CMDSTATE_IDLE		(0)
#define CMDSTATE_SECOND_BYTE	(1)

#define LCD_WIDTH	(128)
#define LCD_HEIGHT	(64)
#define LCD_LEFTBORDER	(0)
#define LCD_RIGHTBORDER	(2)

typedef struct Colorset {
        uint8_t pixval_on[3];
        uint8_t pixval_off[3];
} Colorset;

typedef struct DM128064 {
	uint8_t displayOn;
	uint8_t reverseOn;
	uint8_t entDispOn;
	uint8_t colAdrCntr;
	uint8_t pageAddr;
	uint8_t displayStartLine; /* Scrolling */
	uint8_t readModWrite;
	uint8_t powerCtrl;
	uint8_t shiftReg;
	uint8_t shiftCnt;
	uint8_t biasRatioDenom;
	uint8_t staticIndicatorOn;
	uint8_t muxRatio;
	int scanDirection; /* +-1 */
	float supplyVoltage;
	uint8_t contrast;
	uint8_t gain;
	SigNode *nsigCs1;
	SigTrace *traceCs1;
	SigNode *nsigRes;	
	SigTrace *traceRes;
	SigNode *sigRs;
	SigTrace *traceRs;
	SigNode *sigSclk;
	SigTrace *traceSclk;
	SigNode *sigSid;
	/* Timestamps for timing verification */
	//CycleCounter_t tsAddrSetup;
	int cmd_state;
	uint8_t cmd;	/* only used for 2 byte commands */
	/* 132 * 65 display */
	FbDisplay *display;
	uint8_t gddram[132*9];
	uint16_t win_width;
	uint16_t win_height;
	uint32_t fb_bypp;
	uint32_t fbuffer_size;
	uint32_t fb_linebytes;
	uint32_t fb_lcdlinebytes;
	uint8_t *fbuffer;
	Colorset colors;
	Colorset *colorset;
	CycleTimer updateFbTimer;
} DM128064;

static Colorset colorsets[] =  {
	{
                /* Black on grey (light off) */
                .pixval_on = { 0x10,0x10,0x10},
                .pixval_off = { 0xd8,0xe0,0xd2},
        },
        {
                /* Black on white */
                .pixval_on = { 0x30,0x30,0x30},
                .pixval_off = { 0xff,0xff,0xff},
        },
};

/**
 *********************************************************************
 * \fn static void update_fb(void * clientData); 
 * Send an display update request for the complete display.
 * This is a timer callback routine which gathers all changes within
 * 50ms into one framebuffer update request.
 *********************************************************************
 */
static void update_fb(void * clientData) {
        DM128064 *dm = (DM128064 *) clientData;
        FbUpdateRequest fbudrq;
        fbudrq.offset = 0;
        fbudrq.count = dm->win_width * dm->win_height * dm->fb_bypp;
        fbudrq.fbdata = dm->fbuffer;
        FbDisplay_UpdateRequest(dm->display,&fbudrq);
}

/*
 *********************************************************************************
 * \fn static inline void draw_pixel(DM128064 *dm,uint32_t startaddr,int pixel); 
 * Draw one LCD-Pixel.
 *********************************************************************************
 */
static inline void
draw_pixel(DM128064 *dm,uint32_t startaddr,int pixel) {
        uint32_t addr;
        uint8_t *pixval;
        if(pixel) {
                pixval = dm->colors.pixval_on;
        } else {
                pixval = dm->colors.pixval_off;
        }
	addr = startaddr;
	dm->fbuffer[addr++] = pixval[0];
	dm->fbuffer[addr++] = pixval[1];
	dm->fbuffer[addr++] = pixval[2];
}

/**
 ***************************************************************************
 * redraw all data from gdram to the frame buffer
 ***************************************************************************
 */
static void
redraw_all(DM128064 *dm) {
	int i,j;
	uint32_t addr;
	if(!dm->displayOn) {
		return;
	}
	for(j=0;j < LCD_HEIGHT;j++) {
		addr = j * dm->fb_linebytes;
		for(i=LCD_LEFTBORDER; i < (LCD_LEFTBORDER + LCD_WIDTH);i++) {
			int pixval;
			pixval = (dm->gddram[i + 132 * (j >> 3)] >>  (j & 7)) & 1;
			pixval = (pixval ^ dm->reverseOn) | dm->entDispOn;
			draw_pixel(dm,addr,pixval);
			addr = addr + dm->fb_bypp;
		}
	}
	if(!CycleTimer_IsActive(&dm->updateFbTimer)) {
                CycleTimer_Mod(&dm->updateFbTimer,MillisecondsToCycles(25));
        }
}

static inline void
gdram_addr(DM128064 *dm,int x,int y) 
{
#if 0
	if(dm->comScanDirReverse) {
	}
	addr = x + 132 * ((j) >> 3);
#endif
}

/**
 *******************************************************************************************
 * Redraw a single byte from gdram to the framebuffer
 *******************************************************************************************
 */
static void
redraw_byte(DM128064 *dm,unsigned int x,unsigned int y) {
	int pixval;
	int j;
	uint32_t addr;
	if(!dm->displayOn) {
		return;
	} 
	if((y > LCD_WIDTH) || (x < LCD_LEFTBORDER) || (x > (LCD_LEFTBORDER + LCD_WIDTH))) {
		dbgprintf("Pixel outside of window\n");
		return;
	}
	addr = y * dm->fb_linebytes + (x - LCD_LEFTBORDER) * dm->fb_bypp;
	for(j = y; (j < LCD_HEIGHT) && (j < (y + 8));j++) {
		pixval = (dm->gddram[x + 132 * ((j) >> 3)] >>  ((j) & 7)) & 1;
		pixval = (pixval ^ dm->reverseOn) | dm->entDispOn;
		draw_pixel(dm,addr,pixval);
		addr += dm->fb_linebytes; 
	}
	if(!CycleTimer_IsActive(&dm->updateFbTimer)) {
		CycleTimer_Mod(&dm->updateFbTimer,MillisecondsToCycles(25));
	}
}

/*
 ************************************************ 
 * reverse the image 
 ************************************************ 
 */
static void
reverse_on(DM128064 *dm) {
	if(dm->reverseOn == 0) {
		dm->reverseOn = 1;
		redraw_all(dm);
	}
}

static void
reverse_off(DM128064 *dm) {
	if(dm->reverseOn != 0) {
		dm->reverseOn = 0;
		redraw_all(dm);
	}
}

/*
 **************************************************************************
 * Switch the display off
 **************************************************************************
 */
static void
display_off(DM128064 *dm) 
{
	int i,j;
	uint32_t addr;
	dm->displayOn = 0;
	for(j=0;j < LCD_HEIGHT;j++) {
		addr = j * dm->fb_linebytes;
		for(i=LCD_LEFTBORDER; i < (LCD_LEFTBORDER + LCD_WIDTH);i++) {
			draw_pixel(dm,addr,0);
			addr = addr + dm->fb_bypp;
		}
	}
	if(!CycleTimer_IsActive(&dm->updateFbTimer)) {
                CycleTimer_Mod(&dm->updateFbTimer,MillisecondsToCycles(25));
        }
	
}

static void
display_on(DM128064 *dm) 
{
	dm->displayOn = 1;
	redraw_all(dm);	
}

static void
print_timestamp(DM128064 *dm) {
	dbgprintf("%lld: ",CyclesToMicroseconds(CycleCounter_Get()));
}

/**
 *******************************************************************************
 * \fn static void update_contrast(DM128064 *dm) 
 * update the colors according to contrast and gain settings.
 *******************************************************************************
 */
static void
update_contrast(DM128064 *dm) {
	int i;
	float mul = 4.5 * 31. / (dm->gain + 1) / (dm->contrast + 1);
	mul = mul * pow((3.3 / dm->supplyVoltage),2.5);
	mul = mul * mul * mul; /* Make a little bit nonlinear */
	for(i = 0; i < 3; i++) {
		int32_t on,off;
		on = dm->colorset->pixval_on[i] + ((mul - 1) * 256);
		off = dm->colorset->pixval_off[i] * mul; 
		if(on > 255) {
			on = 255;
		} else if(on < 0) {
			on = 0;
		}
		if(off > 255) {
			off = 255;
		} else if(off < 0) {
			off = 0;
		}
		dm->colors.pixval_on[i] = on;
		dm->colors.pixval_off[i] = off;
	}
}

static void
dm_command(DM128064 *dm,uint8_t cmd) 
{
#if 0
	dm->colAdrCntr = 0;
	dm->displayStartLine = 0;
	dm->pageAddr = 0;
	dm->readModWrite = 0;
	dm->biasRatioDenom = 9;
#endif
	print_timestamp(dm);
	if(dm->cmd_state != CMDSTATE_IDLE) {
		if(dm->cmd == 0x81) {
			dm->contrast = cmd;
			update_contrast(dm);
			redraw_all(dm);
			dbgprintf("Set contrast to %02x\n",dm->contrast);
		} else if(dm->cmd == 0xad) {
			dm->staticIndicatorOn = 1;
			dbgprintf("Indicator is now on, mode %02x\n",cmd);
		} else if(dm->cmd == 0xa9) {
			dbgprintf("BiasTC is now 0x%02x\n",cmd);
		} else if(dm->cmd == 0xa8) {
			dm->muxRatio = cmd & 0x3f;
			dbgprintf("MuxRatio %d\n",dm->muxRatio);
		} else {
			dbgprintf("Got second byte for prev cmd: 0x%02x\n",cmd);
		}
		dm->cmd_state = CMDSTATE_IDLE;
		return;
		// handle second byte
	}
	dm->cmd = cmd;
	if((cmd & 0xf0) == 0x00) {
		dm->colAdrCntr = (dm->colAdrCntr & 0xf0) | (cmd & 0xf);
		dbgprintf("LOWCOLADR: now 0x%02x\n",dm->colAdrCntr);
	} else if((cmd & 0xf0) == 0x10) {
		dm->colAdrCntr = (dm->colAdrCntr & 0xf) | ((cmd & 0xf) << 4);
		dbgprintf("HICOLADR: now 0x%02x\n",dm->colAdrCntr);
	} else if((cmd & 0xf8) == 0x20) {
		dm->gain = cmd & 7;
		update_contrast(dm);
		redraw_all(dm);
		dbgprintf("Internal Resistor Ratio\n");
	} else if((cmd & 0xf8) == 0x28) {
		dbgprintf("Power Control %02x\n",cmd & 7);
		dm->powerCtrl = cmd & 7;
	} else if((cmd & 0xc0) == 0x40) {
		dm->displayStartLine = cmd & 0x3f;
		dbgprintf("Startline 0x%02x\n",dm->displayStartLine);
	} else if((cmd & 0xff) == 0x81) {
		dbgprintf("Contrast\n");
		dm->cmd_state = CMDSTATE_SECOND_BYTE;
	} else if((cmd & 0xfe) == 0xa0) {
		dbgprintf("Segment remap: %d\n",cmd & 0x1);
	} else if((cmd & 0xff) == 0xa2) {
		dbgprintf("LCD Bias 1/9\n");
	} else if((cmd & 0xff) == 0xa3) {
		dbgprintf("LCD Bias 1/7\n");
	} else if((cmd & 0xff) == 0xa4) {
		if(dm->entDispOn) {
			dm->entDispOn = 0;
			redraw_all(dm);
		}
		if(dm->displayOn == 0) {
			fprintf(stderr,"Warning: Display entered Power save mode\n");
		}
		dbgprintf("Entdisp Not On\n");
	} else if((cmd & 0xff) == 0xa5) {
		if(dm->entDispOn == 0) {
			dm->entDispOn = 1;
			redraw_all(dm);
		}
		dbgprintf("Entdisp ON\n");
	} else if((cmd & 0xff) == 0xa6) {
		dbgprintf("Not Reverse\n");
		reverse_off(dm);
	} else if((cmd & 0xff) == 0xa7) {
		dbgprintf("Reverse\n");
		reverse_on(dm);
	} else if((cmd & 0xff) == 0xae) {
		dbgprintf("Display Off\n");
		display_off(dm);
	} else if((cmd & 0xff) == 0xaf) {
		dbgprintf("Display On\n");
		display_on(dm);
	} else if((cmd & 0xf0) == 0xb0) {
		dm->pageAddr = cmd & 0xf;
		dbgprintf("Page Addr %d\n",dm->pageAddr);
	} else if((cmd & 0xf8) == 0xc0) {
		dm->scanDirection = 1;
		dbgprintf("Comscandir Normal\n");
	} else if((cmd & 0xf8) == 0xc8) {
		dm->scanDirection = -1;
		dbgprintf("Comscandir Remap\n");
	} else if((cmd & 0xff) == 0xe0) {
		dbgprintf("Read modify write mode\n");
	} else if((cmd & 0xff) == 0xe2) {
		dbgprintf("Software reset\n");
	} else if((cmd & 0xff) == 0xe0) {
		dbgprintf("End read mod write\n");
	} else if((cmd & 0xff) == 0xac) {
		dbgprintf("Indicator Off\n");
		dm->staticIndicatorOn = 0;
	} else if((cmd & 0xff) == 0xad) {
		dbgprintf("Indicator On\n");
		dm->cmd_state = CMDSTATE_SECOND_BYTE;
	} else if((cmd & 0xff) == 0xe3) {
		dbgprintf("NOP\n");
	} else if((cmd & 0xff) == 0xa8) {
		dbgprintf("SETMUXRATIO\n");
		dm->cmd_state = CMDSTATE_SECOND_BYTE;
	} else if((cmd & 0xff) == 0xa9) {
		dbgprintf("SETBIASTC\n");
		dm->cmd_state = CMDSTATE_SECOND_BYTE;
	} else if((cmd & 0xff) == 0xab) {
		dbgprintf("BIAS 1/4\n");
	} else if((cmd & 0xff) == 0xaa) {
		dbgprintf("BIAS NOT 1/4\n");
	} else if((cmd & 0xff) == 0xd2) {
		dbgprintf("FRAME_PHASES\n");
		dm->cmd_state = CMDSTATE_SECOND_BYTE;
	} else if((cmd & 0xff) == 0xd3) {
		dbgprintf("SET disp offs\n");
		dm->cmd_state = CMDSTATE_SECOND_BYTE;
	} else {
		fprintf(stderr,"DM-LCD: Unknown cmd %02x\n",cmd);
	}	
}

static void
dm_data(DM128064 *dm,uint8_t data) 
{
	uint32_t addr;
	addr =  dm->pageAddr * 132 + dm->colAdrCntr;
	if(addr > sizeof(dm->gddram)) {
		dbgprintf("Out of display RAM: Page %d, col %d\n",dm->pageAddr,dm->colAdrCntr);
		return;
	}
	dm->gddram[addr] = data;
	redraw_byte(dm,dm->colAdrCntr,dm->pageAddr << 3);
	//redraw_all(dm);
	dm->colAdrCntr++;
}

static void
dm_handle_serinput(DM128064 *dm)
{
	uint8_t value = dm->shiftReg;	
	if(SigNode_Val(dm->nsigRes) == SIG_LOW) {
		dbgprintf("ignore %02x because reset is active\n",value);
		return;
	}
	if(SigNode_Val(dm->sigRs) == SIG_LOW) {
		dm_command(dm,value);
	} else {
		print_timestamp(dm);
		dbgprintf("Received display data %02x\n",value);
		dm_data(dm,value);
	}
}
/*
 ********************************************************************
 * sclk_trace
 *      Signal Trace of the Serial Clock line
 *      Shifts the bits.
 ********************************************************************
 */
static void
sclk_trace(SigNode *node,int value,void *clientData)
{
        DM128064 *dm = (DM128064 *) clientData;
	//dbgprintf("Clk trace %d\n",value);
        if(value == SIG_HIGH) {
                if(SigNode_Val(dm->sigSid) == SIG_HIGH) {
                        dm->shiftReg = (dm->shiftReg << 1) | 1;
                } else {
                        dm->shiftReg = (dm->shiftReg << 1);
                }
                dm->shiftCnt++;
                if(dm->shiftCnt == 8) {
			dm_handle_serinput(dm);
                        dm->shiftCnt = 0;
                }
        }
        return;
}

/*
 ************************************************************************
 * \fn static void cs_trace(SigNode *node,int value,void *clientData)
 * Activate/deactivate SPI shift engine depending on CS state.
 * The real device resets the SPI clock counter on chip select.
 ************************************************************************
 */
static void
cs_trace(SigNode *node,int value,void *clientData)
{
        DM128064 *dm = (DM128064 *) clientData;
	//dbgprintf("CS trace %d\n",value);
	if(SigNode_Val(dm->nsigCs1) == SIG_LOW) {
		if(!dm->traceSclk) {
			dm->traceSclk = SigNode_Trace(dm->sigSclk,sclk_trace,dm);
			dm->shiftCnt = 0;
		}
	} else {
		if(dm->traceSclk) {
			SigNode_Untrace(dm->sigSclk,dm->traceSclk);	
			dm->traceSclk = NULL;
		}
	}
}

static void
rs_trace(SigNode *node,int value,void *clientData)
{
	/* 
	 ***********************************************************
	 * The real device does not reset the SPI bitcounter on 
	 * rs edges. Checked 11.07.2012 
	 ***********************************************************
	 */
}

static void
reset_trace(SigNode *node,int value,void *clientData)
{
	DM128064 *dm = clientData;
	print_timestamp(dm);
	if(value == SIG_LOW) {
		dbgprintf("Got reset\n");
	} else {
		dbgprintf("Removed reset\n");
	}
	display_off(dm);
	dm->reverseOn = 0;
	dm->entDispOn = 0;
	dm->colAdrCntr = 0;
	dm->displayStartLine = 0;
	dm->pageAddr = 0;
	dm->readModWrite = 0;
	dm->powerCtrl = 0;
	dm->shiftReg = 0;
	dm->shiftCnt = 0;
	dm->biasRatioDenom = 9;
	dm->staticIndicatorOn = 0;
	dm->scanDirection = 1;
	dm->contrast = 0x20;	
	dm->cmd_state = CMDSTATE_IDLE;
	dm->muxRatio = 0x3f;
}

static void
set_fbformat(FbDisplay *display)
{
        FbFormat fbf;
        fbf.red_bits = 8;
        fbf.red_shift = 0;
        fbf.green_bits = 8;
        fbf.green_shift = 8;
        fbf.blue_bits = 8;
        fbf.blue_shift = 16;
        fbf.bits_per_pixel = 24;
        fbf.depth = 24;
        FbDisplay_SetFbFormat(display,&fbf);
}

void
DM128064_New(const char *name, FbDisplay *display)
{
	DM128064 *dm = sg_new(DM128064);	
	dm->displayOn = 0;
	dm->reverseOn = 0;
	dm->entDispOn = 0;
	dm->colAdrCntr = 0;
	dm->displayStartLine = 0;
	dm->pageAddr = 0;
	dm->readModWrite = 0;
	dm->powerCtrl = 0;
	dm->shiftReg = 0;
	dm->shiftCnt = 0;
	dm->biasRatioDenom = 9;
	dm->staticIndicatorOn = 0;
	dm->scanDirection = 1;
	dm->contrast = 0x20;	
	dm->muxRatio = 0x3f;
	dm->supplyVoltage = 3.3;
	Config_ReadFloat32(&dm->supplyVoltage,name,"vdd");
	dm->nsigCs1 = SigNode_New("%s.cs1",name);;
	dm->traceCs1 = SigNode_Trace(dm->nsigCs1,cs_trace,dm);
	SigNode_Set(dm->nsigCs1,SIG_PULLUP);
	dm->nsigRes = SigNode_New("%s.reset",name);	
	dm->traceRes = SigNode_Trace(dm->nsigRes,reset_trace,dm);
	dm->sigRs = SigNode_New("%s.rs",name);
	dm->traceRs = SigNode_Trace(dm->sigRs,rs_trace,dm);
	dm->sigSclk = SigNode_New("%s.sclk",name);
	dm->sigSid = SigNode_New("%s.sid",name);
	set_fbformat(display);
	dm->display = display;
	dm->win_width =  FbDisplay_Width(display);
        dm->win_height =  FbDisplay_Height(display);
	if((dm->win_width < 128) || (dm->win_height < LCD_HEIGHT)) {
		fprintf(stderr,"Window for LCD to small\n");
		exit(1);
	}
	dm->colorset = &colorsets[0];
	update_contrast(dm);
	dm->fb_bypp = 3;
	dm->fbuffer_size = dm->win_width * dm->win_height * dm->fb_bypp;
        dm->fbuffer = sg_calloc(dm->fbuffer_size);
        dm->fb_linebytes = dm->win_width * dm->fb_bypp;
        dm->fb_lcdlinebytes = dm->fb_linebytes * 1; /* hd->fb_lcdpixheight; */
	CycleTimer_Init(&dm->updateFbTimer,update_fb,dm);
	redraw_all(dm);
}
