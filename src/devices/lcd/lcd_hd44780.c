/*
 ************************************************************************************************
 * Emulation of LCD Text display 
 *
 * state: Working, read+busy polling tested. 
 *
 * Copyright 2009 Jochen Karrer. All rights reserved.
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
#include "sgstring.h"
#include "lcd_hd44780.h"
#include "fbdisplay.h"
#include "cycletimer.h"
#include "configfile.h"

#define REG_ENTRY_S 		(1 << 0)
#define REG_ENTRY_ID 		(1 << 1)
#define REG_DISP_B		(1 << 0)
#define REG_DISP_C		(1 << 1)
#define REG_DISP_D		(1 << 2)
#define REG_SHIFT_RL	(1 << 2)
#define REG_SHIFT_SC	(1 << 3)
#define REG_FUNC_F	(1 << 2)
#define REG_FUNC_N	(1 << 3)
#define REG_FUNC_DL	(1 << 4)

#define DDRAM_SIZE (0x80)
#define DDRAM_MASK (DDRAM_SIZE - 1)
#define CGRAM_SIZE (0x40)
#define CGRAM_MASK (CGRAM_SIZE - 1)

#define CURSOR_MODE_OFF 	(0)
#define CURSOR_MODE_ON 		(1)
#define CURSOR_MODE_BLINK	(3)

#if 0
#define dbgprintf(x...) { fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

typedef enum InterfaceState {
	IFS_IDLE,
	IFS_READING,
} InterfaceState;

typedef struct Colorset {
	uint8_t pixval_bg[3];
	uint8_t pixval_on[3];
	uint8_t pixval_off[3];
	uint8_t pixval_halfon[3];
	uint8_t pixval_halfoff[3];
} Colorset;

/*
 *****************************************************
 * BusTiming
 * Names of timings taken from HD44780U manual.
 *****************************************************
 */
typedef struct BusTiming {
	uint32_t nsTcycE; /**< Enable cycle time */
	uint32_t nsPWeh;  /**< Enable pulse width */	
	uint32_t nsTErEf; /**< Enable rise/fall time */
	uint32_t nsTAS;	  /**< Address set up time */
	uint32_t nsTAH;   /**< Address hold time */
	uint32_t nsTDSW;  /**< Data set up time */
	uint32_t nsTH;	  /**< Data hold time */
	uint32_t nsTDDR;  /**< Data delay time */ 
	uint32_t nsTDHR;  /**< Data hold time */
} BusTiming;

typedef struct ControllerTiming {
	uint32_t usMaxStartupDelay;
	uint32_t usTypStartupDelay;
	uint32_t usMaxDisplayClear;
	uint32_t usTypDisplayClear;
	uint32_t usMaxReturnHome;
	uint32_t usTypReturnHome;
	uint32_t usMaxEntryModeSet;
	uint32_t usTypEntryModeSet;
	uint32_t usMaxDisplayOnOff;
	uint32_t usTypDisplayOnOff;
	uint32_t usMaxShift;
	uint32_t usTypShift;
	uint32_t usMaxFunctionSet;
	uint32_t usTypFunctionSet;
	uint32_t usMaxSetCGRAMaddr;
	uint32_t usTypSetCGRAMaddr;
	uint32_t usMaxSetDDRAMaddr;
	uint32_t usTypSetDDRAMaddr;
	uint32_t usMaxWriteToRAM;
	uint32_t usTypWriteToRAM;
	uint32_t usMaxReadRAM;
	uint32_t usTypReadRAM;
	uint32_t usRWtADD;
} ControllerTiming;

typedef struct HD44780 {
	SigNode *sigRS;
	SigNode *sigRW;
	SigNode *sigE;
	SigNode *sigData[8];	
	int fourbitmode;
	int nibblecounter;
	uint8_t datainreg;
	uint8_t dataoutreg;
	InterfaceState ifstate;
	/* registers accessed with control instruction */
	uint8_t reg_entry;
	uint8_t reg_disp;
	uint8_t reg_shiftcntl;
	uint8_t reg_func;
	uint8_t reg_AC; /* Address counter */
	uint8_t reg_cursorpos;
	uint8_t cursor_blinkstate;
	uint8_t reg_shift;
	uint8_t reg_op_on_cgram;

	uint8_t cgram[CGRAM_SIZE];
	uint8_t ddram[DDRAM_SIZE];

	CycleTimer updateFbTimer;
	CycleTimer cursorBlinkTimer;
	FbDisplay *display;
	Colorset *colorset;
	uint8_t *fbuffer;
	uint32_t fbuffer_size;

	uint32_t fb_bypp;    /* bytes per pixel */
	uint32_t lcd_bypp;    /* bytes per LCD pixel */
	uint32_t lcd_chwidth; /* width in display pixels of one character */
	uint32_t lcd_chheight; /* height in display pixels of one character */
	uint32_t lcd_columns; /* number of characters per line */
	uint32_t lcd_rows;    /* number of rows in display */
	uint32_t lcd_charspace; /* space between two characters in display pixels */ 
	uint32_t lcd_borderwidth; /* left/right/up/down border in display pixels */
	uint32_t lcd_charYdistance; /* Distance of 2 chars in y direction in LCD pixels */
	uint32_t lcd_charXdistance; /* Distance of 2 chars in x direction in LCD pixels */
	uint32_t fb_lcdpixwidth;  /* screen pixel width of one display pixel */
	uint32_t fb_lcdpixheight; /* screen pixel height of one display pixel */
	uint32_t win_width;
	uint32_t win_height;
	uint32_t fb_linebytes;    /* bytes per framebuffer line */
	uint32_t fb_lcdlinebytes; /* size of a one pixel LCD line in framebuffer */

	uint32_t ctrl_cols;    /* controller columns,(not LCD). 40 for 2 line and 20 for 4 line */
	uint32_t ctrl_rows;    /* The number of rows as known by controller */
	uint32_t ctrl_colmask; /* controller row mask (not LCD). Typical 0x3f */

	BusTiming *busTiming; 
	ControllerTiming *ctrlTiming; 
	CycleCounter_t busReadyTime; /* set by interface state change */ 
	CycleCounter_t ctrlReadyTime; /* set by command interpreter */
} HD44780;

typedef struct LCD {
	HD44780 *controller[2];
} LCD;

BusTiming busTimings[] = {
	{
		/* HD44780 timing */
		.nsTcycE = 500,
		.nsPWeh = 230,
		.nsTErEf = 20,
		.nsTAS = 40,
		.nsTAH = 10,
		.nsTDSW = 80,
		.nsTH = 10,
		.nsTDDR = 160,
		.nsTDHR = 5
	},
};

/*
 ********************************************************************
 * Maximum command timings from HD44780U
 * Typical command timings are not from any manual. They
 * are not confirmed.
 ********************************************************************
 */
ControllerTiming controllerTimings[] =  { 
	{
		.usMaxStartupDelay = 30000,
		.usTypStartupDelay = 20000,
		.usMaxDisplayClear = 1530,
		.usTypDisplayClear = 500,
		.usMaxReturnHome = 1520,
		.usTypReturnHome = 500,
		.usMaxEntryModeSet = 37,
		.usTypEntryModeSet = 10,
		.usMaxDisplayOnOff = 37,
		.usTypDisplayOnOff = 10,
		.usMaxShift = 37,
		.usTypShift = 10,
		.usMaxFunctionSet = 37,
		.usTypFunctionSet = 10,
		.usMaxSetCGRAMaddr = 37,
		.usTypSetCGRAMaddr = 10,
		.usMaxSetDDRAMaddr = 37,
		.usTypSetDDRAMaddr = 10,
		.usMaxWriteToRAM = 37,
		.usTypWriteToRAM = 10,
		.usMaxReadRAM = 37,
		.usTypReadRAM = 10,
		.usRWtADD = 4
	}
};

static Colorset colorsets[] =  {
	{
		/* Black on yellow/green */
		.pixval_bg = { 0xb6,0xff,0x55},
		.pixval_on = { 0x24,0x24,0x0},
		.pixval_off = { 0x39,0xef,0x77},
		.pixval_halfon = { 0x49,0x6d,0x00},
		.pixval_halfoff = { 0xB6,0xdb,0x55},
	},
	{
		/* White on blue light */
		.pixval_bg = { 0x48,0x00,0xb0},
		.pixval_on = { 0xff,0xd0,0xe0},
		.pixval_off = { 0x50,0x00,0xa0},
		.pixval_halfon = { 0xb0,0xb0,0xb0},
		.pixval_halfoff = { 0x50,0x00,0xb0},
	},
	{
		/* White on blue dark */
		.pixval_bg = { 0x38,0x20,0x78},
		.pixval_on = { 0xe8,0xe8,0xe8},
		.pixval_off = { 0x48,0x30,0x88},
		.pixval_halfon = { 0xa0,0xa0,0xa0},
		.pixval_halfoff = { 0x30,0x20,0x68},
	},
	{
		/* White on Black */
		.pixval_bg = { 0x20,0x20,0x20},
		.pixval_on = { 0xff,0xff,0xff},
		.pixval_off = { 0x3f,0x3f,0x3f},
		.pixval_halfon = { 0xbb,0xbb,0xbb},
		.pixval_halfoff = { 0x30,0x30,0x30},
	},
	{
		/* Black on white */
		.pixval_bg = { 0xff,0xff,0xff},
		.pixval_on = { 0x30,0x30,0x30},
		.pixval_off = { 0xff,0xff,0xff},
		.pixval_halfoff = { 0xdd,0xdd,0xdd},
		.pixval_halfon = { 0x6f,0x6f,0x6f},
	},
	{
		/* Black on grey (light off) */
		.pixval_bg = { 0x98,0xa2,0x8f},
		.pixval_on = { 0x10,0x10,0x10},
		.pixval_off = { 0x88,0x92,0x7f},
		.pixval_halfoff = { 0x80,0x88,0x78},
		.pixval_halfon = { 0x48,0x48,0x48},
	},
};

/* Taken from DEM 16216 SYH-PY Product specification */
static uint8_t font5x11[256][11] = {
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x00 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x01 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x02 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x03 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x04 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x05 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x06 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x07 is in CG RAM */

	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x08 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x09 is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x0a is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x0b is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x0c is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x0d is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x0e is in CG RAM */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x0f is in CG RAM */
	
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x10 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x11 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x12 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x13 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x14 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x15 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x16 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x17 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x18 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x19 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x1a is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x1b is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x1c is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x1d is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x1e is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x1f is empty */

	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x20 <SPACE> */
	{ 0x04,0x04,0x04,0x04,0x00,0x00,0x04,0x00 }, /* 0x21 Exclamation mark */
	{ 0x0a,0x0a,0x0a,0x00,0x00,0xa0,0x00,0x00 }, /* 0x22 double quote */
	{ 0x0a,0x0a,0x1f,0x0a,0x1f,0x0a,0x0a,0x00 }, /* 0x23 numbersign */ 
	{ 0x04,0x0f,0x14,0x0e,0x05,0x1e,0x04,0x00 }, /* 0x24 dollar */
 	{ 0x18,0x19,0x02,0x04,0x08,0x13,0x03,0x00 }, /* 0x25 percent */
	{ 0x0c,0x12,0x14,0x08,0x15,0x12,0x0d,0x00 }, /* 0x26 ampersand */
	{ 0x0c,0x04,0x08,0x00,0x00,0x00,0x00,0x00 }, /* 0x27 */
	{ 0x02,0x04,0x08,0x08,0x08,0x04,0x02,0x00 }, /* 0x28 parenleft */
	{ 0x08,0x04,0x02,0x02,0x02,0x04,0x08,0x00 }, /* 0x29 parenright */
	{ 0x00,0x04,0x15,0x0e,0x15,0x04,0x00,0x00 }, /* 0x2a asterisk */
	{ 0x00,0x04,0x04,0x1f,0x04,0x04,0x00,0x00 }, /* 0x2b plus */
	{ 0x00,0x00,0x00,0x00,0x0c,0x04,0x08,0x00 }, /* 0x2c comma */
	{ 0x00,0x00,0x00,0x1f,0x00,0x00,0x00,0x00 }, /* 0x2d minus */
	{ 0x00,0x00,0x00,0x00,0x00,0x0c,0x0c,0x00 }, /* 0x2e period */
	{ 0x00,0x01,0x02,0x04,0x08,0x10,0x00,0x00 }, /* 0x2f slash */

	{ 0x0e,0x11,0x13,0x15,0x19,0x11,0x0e,0x00 }, /* 0x30 "0" */
	{ 0x04,0x0c,0x04,0x04,0x04,0x04,0x0e,0x00 }, /* 0x31 "1" */
	{ 0x0e,0x11,0x01,0x02,0x04,0x08,0x1f,0x00 }, /* 0x32 "2" */
	{ 0x1f,0x02,0x04,0x02,0x01,0x11,0x0e,0x00 }, /* 0x33 "3" */	
	{ 0x02,0x06,0x0a,0x12,0x1f,0x02,0x02,0x00 }, /* 0x34 "4" */
	{ 0x1f,0x10,0x1e,0x01,0x01,0x11,0x0e,0x00 }, /* 0x35 "5" */
	{ 0x06,0x08,0x10,0x1e,0x11,0x11,0x0e,0x00 }, /* 0x36 "6" */
 	{ 0x1f,0x01,0x02,0x04,0x08,0x08,0x08,0x00 }, /* 0x37 "7" */
	{ 0x0e,0x11,0x11,0x0e,0x11,0x11,0x0e,0x00 }, /* 0x38 "8" */
	{ 0x0e,0x11,0x11,0x0e,0x01,0x02,0x0c,0x00 }, /* 0x39 "9" */
	{ 0x00,0x0c,0x0c,0x00,0x0c,0x0c,0x00,0x00 }, /* 0x3a colon */	
	{ 0x00,0x0c,0x0c,0x00,0x0c,0x04,0x08,0x00 }, /* 0x3b semicolon */
	{ 0x02,0x04,0x08,0x10,0x08,0x04,0x02,0x00 }, /* 0x3c smaller than */
	{ 0x00,0x00,0x1f,0x00,0x1f,0x00,0x00,0x00 }, /* 0x3d is equal */
	{ 0x08,0x04,0x02,0x01,0x02,0x04,0x08,0x00 }, /* 0x3e greater than */
	{ 0x0e,0x11,0x01,0x02,0x04,0x00,0x04,0x00 }, /* 0x3f question mark */
	
	{ 0x0e,0x11,0x01,0x0d,0x15,0x15,0x0e,0x00 }, /* 0x40 at: "@" */
	{ 0x0e,0x11,0x11,0x11,0x1f,0x11,0x11,0x00 }, /* 0x41 "A" */
	{ 0x1e,0x11,0x11,0x1e,0x11,0x11,0x1e,0x00 }, /* 0x42 "B" */
	{ 0x0e,0x11,0x10,0x10,0x10,0x11,0x0e,0x00 }, /* 0x43 "C" */
	{ 0x1c,0x12,0x11,0x11,0x11,0x12,0x1c,0x00 }, /* 0x44 "D" */
	{ 0x1f,0x10,0x10,0x1e,0x10,0x10,0x1f,0x00 }, /* 0x45 "E" */
	//{ 0x07,0x08,0x1e,0x10,0x1c,0x08,0x07,0x00 }, /* 0x45 "EUR Sie" */
	//{ 0x07,0x08,0x1e,0x10,0x1e,0x08,0x07,0x00 }, /* 0x45 "EUR 1" */
	//{ 0x06,0x09,0x1e,0x8,0x1e,0x09,0x06,0x00 }, /* 0x45 "EUR 2" */
	{ 0x1f,0x10,0x10,0x1e,0x10,0x10,0x10,0x00 }, /* 0x46 "F" */
	{ 0x0e,0x11,0x10,0x17,0x11,0x11,0x0f,0x00 }, /* 0x47 "G" */
	{ 0x11,0x11,0x11,0x1f,0x11,0x11,0x11,0x00 }, /* 0x48 "H" */
	{ 0x0e,0x04,0x04,0x04,0x04,0x04,0x0e,0x00 }, /* 0x49 "I" */
	{ 0x07,0x02,0x02,0x02,0x02,0x12,0x0c,0x00 }, /* 0x4a "J" */
	{ 0x11,0x12,0x14,0x18,0x14,0x12,0x11,0x00 }, /* 0x4b "K" */
	{ 0x10,0x10,0x10,0x10,0x10,0x10,0x1f,0x00 }, /* 0x4c "L" */
	{ 0x11,0x1b,0x15,0x15,0x11,0x11,0x11,0x00 }, /* 0x4d "M" */	
	{ 0x11,0x11,0x19,0x15,0x13,0x11,0x11,0x00 }, /* 0x4e "N" */
	{ 0x0e,0x11,0x11,0x11,0x11,0x11,0x0e,0x00 }, /* 0x4f "O" */

	{ 0x1e,0x11,0x11,0x1e,0x10,0x10,0x10,0x00 }, /* 0x50 "P" */	
	{ 0x0e,0x11,0x11,0x11,0x15,0x12,0x0d,0x00 }, /* 0x51 "Q" */
	{ 0x1e,0x11,0x11,0x1e,0x14,0x12,0x11,0x00 }, /* 0x52 "R" */
	{ 0x0f,0x10,0x10,0x0e,0x01,0x01,0x1e,0x00 }, /* 0x53 "S" */
 	{ 0x1f,0x04,0x04,0x04,0x04,0x04,0x04,0x00 }, /* 0x54 "T" */
	{ 0x11,0x11,0x11,0x11,0x11,0x11,0x0e,0x00 }, /* 0x55 "U" */
	{ 0x11,0x11,0x11,0x11,0x11,0x0a,0x04,0x00 }, /* 0x56 "V" */
	{ 0x11,0x11,0x11,0x15,0x15,0x15,0x0a,0x00 }, /* 0x57 "W" */
	{ 0x11,0x11,0x0a,0x04,0x0a,0x11,0x11,0x00 }, /* 0x58 "X" */
	{ 0x11,0x11,0x11,0x0a,0x04,0x04,0x04,0x00 }, /* 0x59 "Y" */
	{ 0x1f,0x01,0x02,0x04,0x08,0x10,0x1f,0x00 }, /* 0x5a "Z" */
	{ 0x0e,0x08,0x08,0x08,0x08,0x08,0x0e,0x00 }, /* 0x5b "[" */
	{ 0x15,0x0e,0x1f,0x04,0x1f,0x04,0x04,0x00 }, /* 0x5c  Tree */
	{ 0x0e,0x02,0x02,0x02,0x02,0x02,0x0e,0x00 }, /* 0x5d "]" */
	{ 0x04,0x0a,0x11,0x00,0x00,0x00,0x00,0x00 }, /* 0x5e "^" */
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x1f,0x00 }, /* 0x5f "_" */

	{ 0x08,0x04,0x02,0x00,0x00,0x00,0x00,0x00 }, /* 0x60 grave */
	{ 0x00,0x00,0x0e,0x01,0x0f,0x11,0x0f,0x00 }, /* 0x61 "a"   */
	{ 0x10,0x10,0x16,0x19,0x11,0x11,0x1e,0x00 }, /* 0x62 "b"   */
	{ 0x00,0x00,0x0e,0x10,0x10,0x11,0x0e,0x00 }, /* 0x63 "c"   */
	{ 0x01,0x01,0x0d,0x13,0x11,0x11,0x0f,0x00 }, /* 0x64 "d"   */
	{ 0x00,0x00,0x0e,0x11,0x1f,0x10,0x0e,0x00 }, /* 0x65 "e"   */
	{ 0x06,0x09,0x08,0x1c,0x08,0x08,0x08,0x00 }, /* 0x66 "f"   */
	{ 0x00,0x0f,0x11,0x11,0x0f,0x01,0x0e,0x00 }, /* 0x67 "g"   */
	{ 0x10,0x10,0x16,0x19,0x11,0x11,0x11,0x00 }, /* 0x68 "h"   */
	{ 0x04,0x00,0x0c,0x04,0x04,0x04,0x0e,0x00 }, /* 0x69 "i"   */
	{ 0x02,0x00,0x06,0x02,0x02,0x12,0x0c,0x00 }, /* 0x6a "j"   */
	{ 0x10,0x10,0x12,0x14,0x18,0x14,0x12,0x00 }, /* 0x6b "k"   */
	{ 0x0c,0x04,0x04,0x04,0x04,0x04,0x0e,0x00 }, /* 0x6c "l"   */
	{ 0x00,0x00,0x1a,0x15,0x15,0x11,0x11,0x00 }, /* 0x6d "m"   */
	{ 0x00,0x00,0x16,0x19,0x11,0x11,0x11,0x00 }, /* 0x6e "n"   */
	{ 0x00,0x00,0x0e,0x11,0x11,0x11,0x0e,0x00 }, /* 0x6f "o"   */

	{ 0x00,0x00,0x1e,0x11,0x1e,0x10,0x10,0x00 }, /* 0x70 "p"   */
	{ 0x00,0x00,0x0d,0x13,0x0f,0x01,0x01,0x00 }, /* 0x71 "q"   */
	{ 0x00,0x00,0x16,0x18,0x10,0x10,0x10,0x00 }, /* 0x72 "r"   */
	{ 0x00,0x00,0x0e,0x10,0x0e,0x01,0x0e,0x00 }, /* 0x73 "s"   */
	{ 0x08,0x08,0x1c,0x08,0x08,0x09,0x06,0x00 }, /* 0x74 "t"   */
	{ 0x00,0x00,0x11,0x11,0x11,0x13,0x0d,0x00 }, /* 0x75 "u"   */
	{ 0x00,0x00,0x11,0x11,0x11,0x0a,0x04,0x00 }, /* 0x76 "v"   */
	{ 0x00,0x00,0x11,0x11,0x15,0x15,0x0a,0x00 }, /* 0x77 "w"   */
	{ 0x00,0x00,0x11,0x0a,0x04,0x0a,0x11,0x00 }, /* 0x78 "x"   */
	{ 0x00,0x00,0x11,0x11,0x0e,0x01,0x0e,0x00 }, /* 0x79 "y"   */
	{ 0x00,0x00,0x1f,0x02,0x04,0x08,0x1f,0x00 }, /* 0x7a "z"   */
	{ 0x02,0x04,0x04,0x08,0x04,0x04,0x02,0x00 }, /* 0x7b brace left */
	{ 0x04,0x04,0x04,0x04,0x04,0x04,0x04,0x00 }, /* 0x7c bar   */
	{ 0x08,0x04,0x04,0x02,0x04,0x04,0x08,0x00 }, /* 0x7d brace right */
	{ 0x00,0x04,0x02,0x1f,0x02,0x04,0x00,0x00 }, /* 0x7e arrow right */
	{ 0x00,0x04,0x08,0x1f,0x08,0x04,0x00,0x00 }, /* 0x7f arrow left  */
	
	
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x80 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x81 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x82 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x83 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x84 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x85 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x86 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x87 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x88 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x89 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x8a is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x8b is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x8c is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x8d is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x8e is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x8f is empty */

	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x90 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x91 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x92 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x93 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x94 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x95 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x96 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x97 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x98 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x99 is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x9a is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x9b is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x9c is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x9d is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x9e is empty */
	{ 0,0,0,0,0,0,0,0,0,0,0 }, /* 0x9f is empty */

	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0xa0 <Capital SPACE>  */
	{ 0x00,0x00,0x00,0x00,0x1c,0x14,0x1c,0x00 }, /* 0xa1 */
	{ 0x07,0x04,0x04,0x04,0x00,0x00,0x00,0x00 }, /* 0xa2 */
	{ 0x00,0x00,0x00,0x04,0x04,0x04,0x1c,0x00 }, /* 0xa3 */
	{ 0x00,0x00,0x00,0x00,0x10,0x08,0x04,0x00 }, /* 0xa4 */
	{ 0x00,0x00,0x00,0x0c,0x0c,0x00,0x00,0x00 }, /* 0xa5 */
	{ 0x00,0x1f,0x01,0x1f,0x01,0x02,0x04,0x00 }, /* 0xa6 */
 	{ 0x00,0x00,0x1f,0x01,0x06,0x04,0x08,0x00 }, /* 0xa7 */
	{ 0x00,0x00,0x02,0x04,0x0c,0x14,0x04,0x00 }, /* 0xa8 */
	{ 0x00,0x00,0x04,0x1f,0x11,0x01,0x06,0x00 }, /* 0xa9 */
	{ 0x00,0x00,0x00,0x1f,0x04,0x04,0x1f,0x00 }, /* 0xaa */
	{ 0x00,0x00,0x02,0x1f,0x06,0x0a,0x12,0x00 }, /* 0xab */
	{ 0x00,0x00,0x80,0x1f,0x09,0x0a,0x08,0x00 }, /* 0xac */
	{ 0x00,0x00,0x00,0x0e,0x02,0x02,0x1f,0x00 }, /* 0xad */
	{ 0x00,0x00,0x1e,0x02,0x1e,0x02,0x1e,0x00 }, /* 0xae */
	{ 0x00,0x00,0x00,0x15,0x15,0x01,0x06,0x00 }, /* 0xaf */

	{ 0x00,0x00,0x00,0x1f,0x00,0x00,0x00,0x00 }, /* 0xb0 */	
	{ 0x1f,0x01,0x05,0x06,0x04,0x04,0x08,0x00 }, /* 0xb1 */
	{ 0x01,0x02,0x04,0x0c,0x14,0x04,0x04,0x00 }, /* 0xb2 */
	{ 0x04,0x1f,0x11,0x11,0x01,0x02,0x04,0x00 }, /* 0xb3 */
	{ 0x00,0x1f,0x04,0x04,0x04,0x04,0x1f,0x00 }, /* 0xb4 */
	{ 0x02,0x1f,0x02,0x06,0x0a,0x12,0x02,0x00 }, /* 0xb5 */
	{ 0x08,0x1f,0x09,0x09,0x09,0x09,0x12,0x00 }, /* 0xb6 */
	{ 0x04,0x1f,0x04,0x1f,0x04,0x04,0x04,0x00 }, /* 0xb7 */
	{ 0x00,0x0f,0x09,0x09,0x11,0x01,0x02,0x0c }, /* 0xb8 */ 
	{ 0x08,0x0f,0x12,0x02,0x02,0x02,0x04,0x00 }, /* 0xb9 */
	{ 0x00,0x1f,0x01,0x01,0x01,0x01,0x1f,0x00 }, /* 0xba */
	{ 0x0a,0x1f,0x0a,0x0a,0x02,0x04,0x08,0x00 }, /* 0xbb */
	{ 0x00,0x18,0x01,0x19,0x01,0x02,0x1c,0x00 }, /* 0xbc */
	{ 0x00,0x1f,0x01,0x02,0x04,0x0a,0x11,0x00 }, /* 0xbd */
	{ 0x08,0x1f,0x09,0x0a,0x08,0x08,0x07,0x00 }, /* 0xbe */
	{ 0x00,0x11,0x11,0x09,0x01,0x02,0x0c,0x00 }, /* 0xbf */

	{ 0x00,0x0f,0x09,0x15,0x03,0x02,0xc0,0x00 }, /* 0xc0 */
	{ 0x02,0x1c,0x04,0x1f,0x04,0x04,0x80,0x00 }, /* 0xc1 */
	{ 0x00,0x15,0x15,0x15,0x01,0x02,0x04,0x00 }, /* 0xc2 */
	{ 0x0e,0x00,0x1f,0x04,0x04,0x04,0x08,0x00 }, /* 0xc3 */
	{ 0x08,0x08,0x08,0x0c,0x0a,0x08,0x08,0x00 }, /* 0xc4 */
	{ 0x04,0x04,0x1f,0x04,0x04,0x08,0x10,0x00 }, /* 0xc5 */
	{ 0x00,0x0e,0x00,0x00,0x00,0x00,0x1f,0x00 }, /* 0xc6 */
	{ 0x00,0x1f,0x01,0x0a,0x04,0x0a,0x10,0x00 }, /* 0xc7 */
	{ 0x04,0x1f,0x02,0x04,0x0e,0x15,0x04,0x00 }, /* 0xc8 */
	{ 0x02,0x02,0x02,0x02,0x02,0x04,0x08,0x00 }, /* 0xc9 */
	{ 0x00,0x04,0x02,0x11,0x11,0x11,0x11,0x00 }, /* 0xca */
	{ 0x10,0x10,0x1f,0x10,0x10,0x10,0x0f,0x00 }, /* 0xcb */
	{ 0x00,0x1f,0x01,0x01,0x01,0x02,0x0c,0x00 }, /* 0xcc */
	{ 0x00,0x08,0x14,0x02,0x01,0x01,0x00,0x00 }, /* 0xcd */
	{ 0x04,0x1f,0x04,0x04,0x15,0x15,0x04,0x00 }, /* 0xce */
	{ 0x00,0x1f,0x01,0x01,0x0a,0x04,0x02,0x00 }, /* 0xcf */
	
	{ 0x00,0x0e,0x00,0x0e,0x00,0x0e,0x01,0x00 }, /* 0xd0 */
	{ 0x00,0x04,0x08,0x10,0x11,0x1f,0x01,0x00 }, /* 0xd1 */
	{ 0x00,0x01,0x01,0x0a,0x04,0x0a,0x10,0x00 }, /* 0xd2 */
	{ 0x00,0x1f,0x08,0x1f,0x08,0x08,0x07,0x00 }, /* 0xd3 */
	{ 0x08,0x08,0x1f,0x09,0x0a,0x08,0x08,0x00 }, /* 0xd4 */
	{ 0x00,0x0e,0x02,0x02,0x02,0x02,0x1f,0x00 }, /* 0xd5 */
	{ 0x00,0x1f,0x01,0x1f,0x01,0x01,0x1f,0x00 }, /* 0xd6 */
	{ 0x0e,0x00,0x1f,0x01,0x01,0x02,0x04,0x00 }, /* 0xd7 */
	{ 0x12,0x12,0x12,0x12,0x02,0x04,0x08,0x00 }, /* 0xd8 This is wrong in DEM... manuals */
	{ 0x00,0x04,0x14,0x14,0x15,0x15,0x16,0x00 }, /* 0xd9 */
	{ 0x00,0x10,0x10,0x11,0x12,0x14,0x18,0x00 }, /* 0xda */
	{ 0x00,0x1f,0x11,0x11,0x11,0x11,0x1f,0x00 }, /* 0xdb */
	{ 0x00,0x1f,0x11,0x11,0x01,0x02,0x04,0x00 }, /* 0xdc */
	{ 0x00,0x18,0x00,0x01,0x01,0x02,0x1c,0x00 }, /* 0xdd */
	{ 0x04,0x12,0x08,0x00,0x00,0x00,0x00,0x00 }, /* 0xde */
	{ 0x1c,0x14,0x1c,0x00,0x00,0x00,0x00,0x00 }, /* 0xdf */
	
	{ 0x00,0x00,0x09,0x15,0x12,0x12,0x0d,0x00,0x00,0x00,0x00 }, /* 0xe0 */
	{ 0x0a,0x00,0x0e,0x01,0x0f,0x11,0x0f,0x00,0x00,0x00,0x00 }, /* 0xe1 */
	{ 0x00,0x00,0x0e,0x11,0x1e,0x11,0x1e,0x10,0x10,0x10,0x00 }, /* 0xe2 */
	{ 0x00,0x00,0x0e,0x10,0x0c,0x11,0x0e,0x00,0x00,0x00,0x00 }, /* 0xe3 */
	{ 0x00,0x00,0x11,0x11,0x11,0x13,0x1d,0x10,0x10,0x10,0x00 }, /* 0xe4 */
	{ 0x00,0x00,0x0f,0x14,0x12,0x11,0x0e,0x00,0x00,0x00,0x00 }, /* 0xe5 */
	{ 0x00,0x00,0x06,0x09,0x11,0x11,0x1e,0x10,0x10,0x10,0x00 }, /* 0xe6 */	
	{ 0x00,0x00,0x0e,0x11,0x11,0x11,0x0f,0x01,0x01,0x0e,0x00 }, /* 0xe7 */
	{ 0x00,0x00,0x07,0x04,0x04,0x14,0x08,0x00,0x00,0x00,0x00 }, /* 0xe8 */
	{ 0x00,0x02,0x1a,0x02,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0xe9 */
	{ 0x02,0x00,0x06,0x02,0x02,0x02,0x02,0x02,0x12,0x0c,0x00 }, /* 0xea */
	{ 0x00,0x14,0x08,0x14,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0xeb */
	{ 0x00,0x04,0x0e,0x14,0x15,0x0e,0x04,0x00,0x00,0x00,0x00 }, /* 0xec */
	{ 0x08,0x08,0x1c,0x08,0x1c,0x08,0x0f,0x00,0x00,0x00,0x00 }, /* 0xed */
	{ 0x0e,0x00,0x16,0x19,0x11,0x11,0x11,0x00,0x00,0x00,0x00 }, /* 0xee */
	{ 0x0a,0x00,0x0e,0x11,0x11,0x11,0x0e,0x00,0x00,0x00,0x00 }, /* 0xef */

	{ 0x00,0x00,0x16,0x19,0x11,0x11,0x1e,0x10,0x10,0x10,0x00 }, /* 0xf0 */
	{ 0x00,0x00,0x0d,0x13,0x11,0x11,0x0f,0x01,0x01,0x01,0x00 }, /* 0xf1 */
	{ 0x00,0x0e,0x11,0x1f,0x11,0x11,0x0e,0x00,0x00,0x00,0x00 }, /* 0xf2 */
	{ 0x00,0x00,0x00,0x0b,0x15,0x1a,0x00,0x00,0x00,0x00,0x00 }, /* 0xf3 */
	{ 0x00,0x00,0x0e,0x11,0x11,0x0a,0x1b,0x00,0x00,0x00,0x00 }, /* 0xf4 */
	{ 0x0a,0x00,0x11,0x11,0x11,0x13,0x0d,0x00,0x00,0x00,0x00 }, /* 0xf5 */
	{ 0x1f,0x10,0x08,0x04,0x08,0x10,0x1f,0x00,0x00,0x00,0x00 }, /* 0xf6 */	
	{ 0x00,0x00,0x1f,0x0a,0x0a,0x0a,0x11,0x00,0x00,0x00,0x00 }, /* 0xf7 */
	{ 0x1f,0x00,0x11,0x0a,0x04,0x0a,0x11,0x00,0x00,0x00,0x00 }, /* 0xf8 */
	{ 0x00,0x00,0x11,0x11,0x11,0x11,0x0f,0x01,0x01,0x0e,0x00 }, /* 0xf9 */
	{ 0x00,0x01,0x1f,0x04,0x1f,0x04,0x04,0x00,0x00,0x00,0x00 }, /* 0xfa */
	{ 0x00,0x00,0x1f,0x08,0x0f,0x09,0x11,0x00,0x00,0x00,0x00 }, /* 0xfb */
	{ 0x00,0x00,0x1f,0x15,0x1f,0x11,0x11,0x00,0x00,0x00,0x00 }, /* 0xfc */
	{ 0x00,0x00,0x04,0x00,0x1f,0x00,0x04,0x00,0x00,0x00,0x00 }, /* 0xfd */
	{ 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00 }, /* 0xfe */
	{ 0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x1f,0x00 }, /* 0xff */

	
};

static void __attribute__((unused)) dump_ppm( HD44780 *hd) {
	int i;
	FILE *file;
	static int counter = 0;
	char *filename = alloca(100);
	sprintf(filename,"shot_%03d.ppm",counter);
	file = fopen(filename,"w+");
	if(!file) {
		return;
	}
	fprintf (file, "P6\n%i %i\n255\n", hd->win_width,hd->win_height);
	for(i=0;i<hd->win_height;i++) {
		fwrite(hd->fbuffer+i*hd->win_width * 3,1,hd->win_width*3,file);
	}
	fclose(file);
	counter++;
}

static inline void
make_bus_busy(HD44780 *hd,uint32_t nsTime) 
{
	hd->busReadyTime = CycleCounter_Get() + NanosecondsToCycles(nsTime);
}

static inline void
make_controller_busy(HD44780 *hd,uint32_t usTime) 
{
	dbgprintf("Require %d us\n",usTime);
	hd->ctrlReadyTime = CycleCounter_Get() + MicrosecondsToCycles(usTime);
}

static inline int 
check_controller_busy(HD44780 *hd) 
{
	return CycleCounter_Get() < hd->ctrlReadyTime;
}

static inline int 
check_bus_busy(HD44780 *hd) 
{
	return CycleCounter_Get() < hd->busReadyTime;
}

/**
 *********************************************************************
 * \fn static void update_fb(void * clientData); 
 * Send an display update request for the complete display.
 * This is a timer callback routine which gathers all changes within
 * 50ms into one framebuffer update request.
 *********************************************************************
 */
static void update_fb(void * clientData) {
	HD44780 *hd = (HD44780 *) clientData;	
	FbUpdateRequest fbudrq;
	fbudrq.offset = 0; 
	fbudrq.count = hd->win_width * hd->win_height * hd->fb_bypp; 
	fbudrq.fbdata = hd->fbuffer;
	FbDisplay_UpdateRequest(hd->display,&fbudrq);
	//dump_ppm(hd); 
}

/*
 *********************************************************************************
 * \fn static inline void draw_pixel(HD44780 *hd,uint32_t startaddr,int pixel); 
 * Draw one LCD-Pixel.
 *********************************************************************************
 */
static inline void
draw_pixel(HD44780 *hd,uint32_t startaddr,int pixel) {
	int i,j;
	uint32_t addr;
	uint8_t *pixval; 
	uint8_t *pixhalf; 
	if(pixel) {
		pixval = hd->colorset->pixval_on;
		pixhalf = hd->colorset->pixval_halfon;
	} else {
		pixval = hd->colorset->pixval_off;
		pixhalf = hd->colorset->pixval_halfoff;
	}
	for(i=0;i < hd->fb_lcdpixheight;i++) {
		addr = startaddr + i * hd->fb_linebytes;
		for(j = 0;j < hd->fb_lcdpixwidth; j++) {
			if((j == 0) || (i == 0)) {
				hd->fbuffer[addr + 0] = pixhalf[0];
				hd->fbuffer[addr + 1] = pixhalf[1];
				hd->fbuffer[addr + 2] = pixhalf[2];
				addr += hd->fb_bypp;
			} else {
				hd->fbuffer[addr + 0] = pixval[0];	
				hd->fbuffer[addr + 1] = pixval[1];	
				hd->fbuffer[addr + 2] = pixval[2];	
				addr += hd->fb_bypp;
			}
		}
	}
}
 
/**
 *************************************************************************************************
 * \fn static void draw_character(HD44780 * hd,uint8_t c,uint32_t xpos,uint32_t ypos) 
 * Draw one character onto the LC-display.
 *************************************************************************************************
 */
static void draw_character(HD44780 * hd,uint8_t c,uint32_t xpos,uint32_t ypos,int is_cursor) 
{
	uint32_t first_byte;
	uint32_t addr;
	uint32_t endplusone;
	uint8_t *chardata;
	int i,j;
	//dbgprintf("Draw character 0x%02x at %d,%d \n",c,xpos,ypos);
	if((xpos >= hd->lcd_columns) || (ypos >= hd->lcd_rows)) {
		//dbgprintf("Ignoring %d %d, ctrl_rows %d\n",xpos,ypos,hd->ctrl_rows);
		return;
	}
	first_byte = hd->fb_bypp * hd->fb_lcdpixwidth * (hd->lcd_borderwidth + xpos * hd->lcd_charXdistance)
		+(((ypos * hd->lcd_charYdistance) + hd->lcd_borderwidth) * hd->fb_lcdlinebytes)  ;

	endplusone = first_byte + (hd->lcd_chheight * hd->lcd_chwidth) * hd->lcd_bypp;
	if(endplusone > hd->fbuffer_size) {
		fprintf(stderr,"Out of framebuffer\n");
		return;
	}
	if(c < 16) {
		chardata = &hd->cgram[(c & 7) << 3];
	} else {
		chardata = font5x11[c];
	}
	for(i = 0;i < hd->lcd_chheight; i++) {
		uint8_t b;
		if(is_cursor) {
			if(hd->cursor_blinkstate == 1) {
				b = 0x1f;
			} else if ((i == (hd->lcd_chheight - 1)) && (hd->reg_disp & REG_DISP_C)) {
				b = 0x1f;
			} else {
				b = chardata[i];
			}
		} else {
			b = chardata[i];
		}
		addr = first_byte + (i * hd->fb_lcdpixheight * hd->fb_linebytes);
		for(j = 0; j < hd->lcd_chwidth ;j++) {
			int val = (b >> (4-j)) & 1;
			draw_pixel(hd,addr,val); 
			addr += hd->fb_lcdpixwidth * hd->fb_bypp;
		}
	}	
	if(!CycleTimer_IsActive(&hd->updateFbTimer)) {
		CycleTimer_Mod(&hd->updateFbTimer,MillisecondsToCycles(30));
	}

}

/*
 *********************************************************************
 * Update the character at a specified address counter location
 * in display.
 *********************************************************************
 */
static void
update_char(HD44780 *hd,uint8_t ddaddr)
{
	uint8_t character; 
	uint8_t ctrl_row,disp_row,disp_col;
	/* Two line display first */
	if((ddaddr & hd->ctrl_colmask) >= hd->ctrl_cols) {
		//dbgprintf("ddram addr %02x does not match ctrl_colmask\n",ddaddr);
		return;
	} 
	disp_col = (ddaddr & hd->ctrl_colmask) % hd->ctrl_cols;
	if(hd->lcd_rows == 2) {
		ctrl_row = disp_row = (ddaddr & 0x40) >> 6;
		disp_col = (disp_col + hd->reg_shift) % 40;
	} else if(hd->lcd_rows == 4) {
		disp_col = (disp_col + hd->reg_shift) % 40;
		ctrl_row = disp_row = (ddaddr & 0x40) >> 6;
		if(disp_col >= 20) {
			disp_col -= 20;
			disp_row += 2;
		}
	} else {
		ctrl_row = disp_row = disp_col = 0;
	}
	
	if(hd->reg_disp & REG_DISP_D) {
		character = hd->ddram[ddaddr];
	} else {
		character = 0x20;
	}
	if(ctrl_row < hd->ctrl_rows) {
		draw_character(hd,character,disp_col,disp_row,ddaddr == hd->reg_cursorpos);
	} else {
		//dbgprintf("Row for char 0x%02x is not active\n",ddaddr);
	}
}

static void
redraw_all(HD44780 *hd) {
	int i;
	for(i=0;i<sizeof(hd->ddram);i++) {
		update_char(hd,i);
	}
}

/*
 ************************************************************
 * Fill the framebuffer with background color and draw 
 * space characters.
 ************************************************************
 */
static void clear_display(HD44780 *hd) {
	int i;
	for(i=0;i<hd->fbuffer_size;i+=hd->fb_bypp) {
		hd->fbuffer[i] = hd->colorset->pixval_bg[0];
		hd->fbuffer[i+1] = hd->colorset->pixval_bg[1];
		hd->fbuffer[i+2] = hd->colorset->pixval_bg[2];
	}
	memset(hd->ddram,0x20,sizeof(hd->ddram));
	redraw_all(hd);
}
/*
 **********************************************************
 * Timer routine which inverts the cursor every 300 ms
 **********************************************************
 */
static void cursor_blink(void *cd) {
	HD44780 *hd = (HD44780 *) cd;
	if(hd->reg_disp & REG_DISP_B) {
		hd->cursor_blinkstate ^= 1;
		dbgprintf("Cursor at %02x Blink to %d\n",hd->reg_cursorpos,hd->cursor_blinkstate);
		if(!CycleTimer_IsActive(&hd->cursorBlinkTimer)) {
			CycleTimer_Mod(&hd->cursorBlinkTimer,MillisecondsToCycles(300));
		}
		if(hd->reg_cursorpos != 0xff) {
			dbgprintf("Update Cursor at %d to %d\n",hd->reg_cursorpos,hd->cursor_blinkstate);
			update_char(hd,hd->reg_cursorpos);
		}
	}
}

/*
 ***************************************************************
 * Make the cursor address illegal and redraw the character 
 * under the cursor if necessary.
 ***************************************************************
 */
static void
cursor_undraw(HD44780 *hd) {
	uint8_t addr = hd->reg_cursorpos;
	hd->reg_cursorpos = 0xff;	
	if(hd->reg_disp & (REG_DISP_B | REG_DISP_C)) {
		update_char(hd,addr);
	}
}

/*
 **************************************************************
 * Set the cursorposition and redraw the character at the
 * cursorposition.
 **************************************************************
 */
static void
cursor_set(HD44780 *hd,uint8_t addr) {
	hd->reg_cursorpos = addr;	
	if(hd->reg_disp & (REG_DISP_B | REG_DISP_C)) {
		dbgprintf("Cursor is On %02x, addr %02x\n",hd->reg_AC,addr);
		update_char(hd,addr);
	}
	if(hd->reg_disp & REG_DISP_B) {
		if(!CycleTimer_IsActive(&hd->cursorBlinkTimer)) {
			dbgprintf("Activate blinktimer\n");
			CycleTimer_Mod(&hd->cursorBlinkTimer,MillisecondsToCycles(150));
		}
	}
}

/* 
 ***********************************************
 * redraw cgchar whenever the cgram changes 
 ***********************************************
 */
static void
redraw_cgramchar(HD44780 *hd,uint8_t c) {
	int i;
	for(i=0;i<sizeof(hd->ddram);i++) {
		if(hd->ddram[i] == c) {
			update_char(hd,i);
		}
	}
}
/**
 ******************************************************************
 * \fn static void addr_counter_incdec(HD44780 *hd,int add); 
 * Increment/decrement address counter and normalize it.
 * \param hd  Pointer to the HD44780 Display.
 * \param add Integer added to the Address counter "reg_AC".
 ******************************************************************
 */

static void addr_counter_incdec(HD44780 *hd,int add) {
	int row,column,delta_row;	
	/* 
	 **************************************************************************************
	 * Check if reg_entry was initialized. This is because some HD44780 compatible devices
	 * have a a default contents of "decrement" in the direction register.
	 **************************************************************************************
	 */
	if(!(hd->reg_entry & 4)) {
		fprintf(stderr,"LCD warning: Using uninitialized "
			"Increment/Decrement in Entry register\n"); 
		hd->reg_entry |= 4;
	}
	/* CGRAM case is easy */
	if(hd->reg_op_on_cgram) {
		hd->reg_AC = (hd->reg_AC + add) & (sizeof(hd->cgram) - 1);
		return;
	}
	column = (hd->reg_AC & hd->ctrl_colmask); /* This is the controller column */
	delta_row =  ((column + hd->ctrl_cols + add) / hd->ctrl_cols) - 1;
	/* 
	 ****************************************************
 	 * Real device writes to the upper left corner
	 * if address counter is set to 0x40 + 0x28 + 1.
	 * This seems to be a comparator which resets column 
	 * if > 40 
	 ****************************************************
	 */
	if(delta_row == 1) {
		column = 0;
	} else if(delta_row == -1) {
		column = hd->ctrl_cols - 1;
	} else {
		column = (column + hd->ctrl_cols + add) % hd->ctrl_cols;
	}
	/* 
 	 ************************************************************
         * The real device adds the overflow from modulo
	 * to the address counter divided by controller columns 
	 * Tested with LM162551 
 	 ************************************************************
	 */

	row = (hd->reg_AC >> 6) & 1;
	/*
	 *************************************************
	 * Real device really adds this up: 
	 * An address counter of 0x40 + 0x28 
	 * writes into the upper left corner ! 
	 *************************************************
	 */
	row = (row + delta_row) & 1;
	hd->reg_AC = column + (0x40) * row;
	dbgprintf("After add %d AC %d\n",add,hd->reg_AC);
}

/*
 *********************************************************
 * suck a byte of data from the controller
 *********************************************************
 */
static uint8_t 
suck_controller(HD44780 *hd,int rs) 
{
	uint8_t data;
	if(hd->fourbitmode) {
		if(hd->nibblecounter & 1) {
			hd->nibblecounter = 0;
			return (hd->dataoutreg & 0xf) << 4;
		} else {
			hd->nibblecounter = 1;
		}
	}
	if(rs == 0) {
		data = hd->reg_AC & 0x7f;
		if(check_controller_busy(hd)) {
			data |= 0x80;
		}
	} else {
		if(check_controller_busy(hd)) {
			static int counter = 0;
			/* First might be a glitch from enabling GPIO-Pins */
			if((counter > 0) && (counter < 10)) {
				fprintf(stderr,"Text-LCD Read coming too soon\n");
			}
			counter++;
			/* Don't know what the real device does return */
			return hd->reg_AC | 0x80;
		}
		if(hd->reg_op_on_cgram) {
			data = hd->cgram[hd->reg_AC & CGRAM_MASK];
		} else {
			data = hd->ddram[hd->reg_AC & DDRAM_MASK];
		}
		if(hd->reg_entry & REG_ENTRY_ID) {
			addr_counter_incdec(hd,1);
		} else {
			addr_counter_incdec(hd,-1);
		}
		dbgprintf("Require Display Clear time\n");
		make_controller_busy(hd,hd->ctrlTiming->usTypDisplayClear);
	}	
	hd->dataoutreg = data;
	return data & 0xf0;
}

/*
 **********************************************************
 * Feed the controller with data. Called whenever
 * a negative edge on the enable line is detected.
 **********************************************************
 */
static void
feed_controller(HD44780 *hd,int rs,uint8_t indata) 
{
	uint8_t data;
	if(hd->fourbitmode) {
		if(hd->nibblecounter & 1) {
			hd->datainreg |= (indata >> 4);			
			hd->nibblecounter = 0;
			dbgprintf("low nible %02x\n",hd->datainreg);
		} else {
			hd->datainreg = indata & 0xf0;			
			hd->nibblecounter = 1;
			dbgprintf("high nible %02x\n",hd->datainreg);
			return;
		}
	} else {
		hd->datainreg = indata;
	}
	data = hd->datainreg;
	if(check_controller_busy(hd)) {
		static int counter = 0;
		/* First might be a glitch from enabling GPIO-Pins */
		if((counter > 0) && (counter < 10)) {
			fprintf(stderr,"Text-LCD rs %d data 0x%02x coming too soon by %d Cycles\n", 
			rs,data,(int)(hd->ctrlReadyTime - CycleCounter_Get()));
		}
		counter++;
		return;
	}
	if(rs == SIG_LOW) {
		/* It is a command */
		dbgprintf("Command 0x%02x\n",data);
		if(data == 1) {
			/* Display Clear */
			int i;
			for(i=0;i < sizeof(hd->ddram);i++) {
				hd->ddram[i] = 0x20;	
			}
			hd->reg_AC = 0;
			hd->reg_op_on_cgram = 0;
			redraw_all(hd);
			dbgprintf("Require Display Clear time\n");
			make_controller_busy(hd,hd->ctrlTiming->usTypDisplayClear);
		} else if((data & 0xfe) == 0x2) {
			/* Display/Cursor Home */
			cursor_undraw(hd);
			hd->reg_AC = 0;
			hd->reg_shift = 0;
			cursor_set(hd,0);
			redraw_all(hd);
			dbgprintf("Require Display Return home time\n");
			make_controller_busy(hd,hd->ctrlTiming->usTypReturnHome);
		} else if((data & 0xfc) == 0x4) {
			/* Entry mode set */
			hd->reg_entry = data;
			dbgprintf("EM 0x%02x Require TypeEntryModeSet time\n",data);
			make_controller_busy(hd,hd->ctrlTiming->usTypEntryModeSet);
		} else if((data & 0xf8) == 0x8) {
			/* Display On/Off */
			cursor_undraw(hd);
			hd->reg_disp = data;
			if(!hd->reg_op_on_cgram) {
				cursor_set(hd,hd->reg_AC);
			}
			dbgprintf("Require TypDisplayOnOff time\n");
			make_controller_busy(hd,hd->ctrlTiming->usTypDisplayOnOff);
		} else if((data & 0xf0) == 0x10) {
			/* Display/cursor shift */
			hd->reg_shiftcntl = data;
			if(data & REG_SHIFT_SC) {
				/* Shift data */
				if(data & REG_SHIFT_RL) {
					hd->reg_shift = (hd->reg_shift + 1) 
						% hd->ctrl_cols;
				} else {
					hd->reg_shift = (hd->reg_shift + 
						hd->ctrl_cols - 1)
						% hd->ctrl_cols;
				}
				redraw_all(hd);
			} else {
				/* Does cursor move work when CGRAM is selected ? */
				if(hd->reg_op_on_cgram) {
					cursor_undraw(hd);	
				}
				if(data & REG_SHIFT_RL) {
					addr_counter_incdec(hd,1);
				} else {
					addr_counter_incdec(hd,-1);
				}
				if(!hd->reg_op_on_cgram) {
					cursor_set(hd,hd->reg_AC);
				}
			}
			dbgprintf("Require ShiftTime\n");
			make_controller_busy(hd,hd->ctrlTiming->usTypShift);
		} else if((data & 0xe0) == 0x20) {
			/* Function Set */
			if(data & REG_FUNC_DL) {
				hd->fourbitmode = 0;
			} else {
				hd->fourbitmode = 1;
				hd->nibblecounter = 0;
			}
			hd->reg_func = data;
			if(data & REG_FUNC_N) { 
				hd->ctrl_rows = 2;
			} else {
				hd->ctrl_rows = 1;
			}
			redraw_all(hd);
			dbgprintf("Func 0x%02x Require FunctionSet Time\n",data);
			dbgprintf("Fourbitmode %d, nibble %d\n",hd->fourbitmode,hd->nibblecounter);
			make_controller_busy(hd,hd->ctrlTiming->usTypFunctionSet);
		} else if((data & 0xc0) == 0x40) {
			/* CG RAM address set */
			hd->reg_AC = data & 0x3f;
			hd->reg_op_on_cgram = 1;
			dbgprintf("Require SetCGRAM Time\n");
			make_controller_busy(hd,hd->ctrlTiming->usTypSetCGRAMaddr);
		} else if((data & 0x80) == 0x80) {
			/* 
			 ***************************************
			 * DD Ram address set 
			 ***************************************
			 */
			cursor_undraw(hd);
			hd->reg_AC = data & DDRAM_MASK;
			hd->reg_op_on_cgram = 0;
			/* 
			 * It seems to set the cursor before normalization.
			 * This needs a check with a position outside of range (0xf0 for example)
			 */
			cursor_set(hd,hd->reg_AC);
			addr_counter_incdec(hd,0); /* normalize */
			dbgprintf("Require SetDDRAM Time\n");
			make_controller_busy(hd,hd->ctrlTiming->usTypSetDDRAMaddr);
			/*
			 ***********************************************
			 * Not clear if this sets the cursor for all
			 * displays. At least it does for one !
			 ***********************************************
			 */
		}
	} else if(rs == SIG_HIGH) {
		/* It is a data byte */
		dbgprintf("DATA 0x%02x\n",data);
		if(hd->reg_op_on_cgram) {
			uint8_t addr = hd->reg_AC & 0x3f;
			hd->cgram[addr] = data;
			redraw_cgramchar(hd,addr / 8);
			dbgprintf("Written to CGRAM\n");
		} else {
			dbgprintf("Write to AC %02x, shift %d, value 0x%02x\n",hd->reg_AC,hd->reg_shift,data);
			cursor_undraw(hd);
			hd->ddram[hd->reg_AC & DDRAM_MASK] = data;
			if(hd->reg_entry & REG_ENTRY_S) {
				hd->reg_shift = (hd->reg_shift + hd->ctrl_cols  - 1) % hd->ctrl_cols;
				redraw_all(hd);
			} else {
				update_char(hd,hd->reg_AC);
			}
		}
		if(hd->reg_entry & REG_ENTRY_ID) {
			addr_counter_incdec(hd,1);
		} else {
			addr_counter_incdec(hd,-1);
		}
		if(!hd->reg_op_on_cgram) {
			cursor_set(hd,hd->reg_AC);
		}
		dbgprintf("Require WriteToRAM Time\n");
		make_controller_busy(hd,hd->ctrlTiming->usTypWriteToRAM);
	}
}

/**
 **************************************************************
 * The main state machine is driven by level changes of the
 * Enable Signal Line (sigE).
 * Timings taken from DEM20485SYH-LY.pdf
 **************************************************************
 */
static void sigE_Trace(SigNode *node,int value,void *clientData)
{
        HD44780 *hd = (HD44780 *) clientData;
	uint8_t data = 0;
	int i;
	int rw = SigNode_Val(hd->sigRW);
	int rs = SigNode_Val(hd->sigRS);
	int e = value; 
	if(check_bus_busy(hd)) {
		static int counter = 0;
		if(counter++ < 10) {
			uint32_t ns = CyclesToNanoseconds(hd->busReadyTime - CycleCounter_Get());
			fprintf(stderr,"Timing of Text-LCD \"sigE\" is to fast by %uns, E:%d RW:%d RS: %d\n",ns,e,rw,rs);
		}
		return;
	}
	switch(hd->ifstate)  {
		case IFS_READING:
			if((e == SIG_LOW)) {
				/* Release the data lines */
				/* Hold time should be 5 ns */
				for(i=0;i<8;i++) {
					SigNode_Set(hd->sigData[i],SIG_OPEN);
				}
				hd->ifstate = IFS_IDLE;	
			}
			/* No break, fall through ! */
		case IFS_IDLE:
			/* Sample on negative E-Edge */
			if((e == SIG_LOW) && (rw == SIG_LOW)) {
				/* 
				 **********************************************
				 * RW should be low for 40 ns already 
				 **********************************************
				 */
				for(i=0;i<8;i++) {
					if(SigNode_Val(hd->sigData[i]) == SIG_HIGH) {
						data |= (1<<i);
					}
				}
				dbgprintf("sample on neg E edge %02x\n",data);
				feed_controller(hd,rs,data);	
			} else if((e == SIG_HIGH) && (rw == SIG_HIGH)) {
				/* 
				 **********************************************
				 * RW should be high for 40 ns already 
				 *  New data will be available after 120 ns.
				 **********************************************
				 */
				hd->ifstate = IFS_READING;
				data = suck_controller(hd,rs);
				//dbgprintf("suck on pos E edge: %02x\n",data);
				for(i=0;i<8;i++) {
					if(data & (1<<i)) {
						SigNode_Set(hd->sigData[i],SIG_HIGH);
					} else {
						SigNode_Set(hd->sigData[i],SIG_LOW);
					}
				}
			}
			break;
	}
	make_bus_busy(hd,hd->busTiming->nsTcycE >> 1);
}

static void
set_fbformat(HD44780 *hd)
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
        FbDisplay_SetFbFormat(hd->display,&fbf);
}


static HD44780 *
HD44780_New(const char *name,FbDisplay *display,int win_height,int win_width) {
	HD44780 *hd;
	uint32_t colorset_nr = 0;
	int i;
	hd = sg_new(HD44780);	
	hd->sigRS = SigNode_New("%s.RS",name);	
	hd->sigRW = SigNode_New("%s.RW",name);	
	hd->sigE = SigNode_New("%s.E",name);	
	if(!hd->sigRS || !hd->sigRW || !hd->sigE) {
		fprintf(stderr,"Can not create signal for LCD controller %s\n",name);
		exit(1);
	}
	for(i = 0; i < 8;i++) {
		hd->sigData[i] = SigNode_New("%s.D%d",name,i);	
		if(!hd->sigData[i]) {
			fprintf(stderr,
			"Can not create data line for LCD controller %s\n",name);
			exit(1);
		}
	}
	SigNode_Trace(hd->sigE,sigE_Trace,hd);
	hd->reg_cursorpos = 0xff;
	/* Tested with real chip: default cursor mode is increment */
	hd->reg_entry =  REG_ENTRY_ID; 
	hd->cursor_blinkstate = 0;
	hd->fourbitmode = 0;
	hd->display = display;
	hd->fb_bypp = 3;
	hd->lcd_chwidth = 5;
	hd->lcd_chheight = 8;
	hd->lcd_charspace = 1;

	hd->lcd_borderwidth = 4;
	hd->fb_lcdpixwidth = 3;
	hd->fb_lcdpixheight = 3;
	hd->lcd_bypp = hd->fb_bypp * hd->fb_lcdpixwidth * hd->fb_lcdpixheight;

	hd->lcd_rows = 2;	
	Config_ReadUInt32(&hd->lcd_rows,name,"rows");
	hd->ctrl_rows = 1;	
	hd->ctrl_cols = 40;
	hd->ctrl_colmask = 0x3f;
	hd->lcd_columns = 16;
	Config_ReadUInt32(&hd->lcd_columns,name,"cols");
	set_fbformat(hd);

	hd->win_width =  win_width; 
        hd->win_height =  win_height;
        hd->fbuffer_size = hd->win_width * hd->win_height * hd->fb_bypp;
	hd->fbuffer = sg_calloc(hd->fbuffer_size);
	hd->fb_linebytes = hd->win_width * hd->fb_bypp;
	hd->lcd_charYdistance = hd->lcd_chheight + hd->lcd_charspace;
	hd->lcd_charXdistance = hd->lcd_chwidth + hd->lcd_charspace;
	hd->fb_lcdlinebytes = hd->fb_linebytes * hd->fb_lcdpixheight;
	CycleTimer_Init(&hd->updateFbTimer,update_fb,hd);
	CycleTimer_Init(&hd->cursorBlinkTimer,cursor_blink,hd);
	Config_ReadUInt32(&colorset_nr,name,"colorset");
	colorset_nr = colorset_nr % array_size(colorsets);
	hd->colorset = &colorsets[colorset_nr];
	hd->busTiming = &busTimings[0]; 
	hd->ctrlTiming = &controllerTimings[0]; 
	clear_display(hd);
	Config_ReadUInt32(&hd->ctrlTiming->usTypStartupDelay,name,"startupdelay");
	dbgprintf("Require StartupDelay time\n");
	make_controller_busy(hd,hd->ctrlTiming->usTypStartupDelay);
	make_bus_busy(hd,0);
	fprintf(stderr,"LCD Display \"%s\" created\n",name);
	return hd;
}

void
HD44780_LcdNew(const char *name, FbDisplay *display)
{
	LCD *lcd;
	uint32_t win_width,win_height;
	if(!display) {
		fprintf(stderr,"No display for LCD controller\n");
		exit(1);
	}
	lcd = sg_new(LCD);
	win_width =  FbDisplay_Width(display);
        win_height =  FbDisplay_Height(display);
	lcd->controller[0] = HD44780_New(name,display,win_height,win_width); 
}
