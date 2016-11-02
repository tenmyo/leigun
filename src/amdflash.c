/*
 **************************************************************************************************
 *
 * Emulation of Flashbanks with AMD Compatible Flash Chips
 * (Primary Command Set 2) with or without CFI Interface 
 * in 1x16 or 2x16 Bit Mode
 *
 * State: 
 *	Working, SecSi and Erase Suspend is missing
 *	Destructors missing, Write should need some
 *	time (like in real device) instead of beeing acknowledged
 *	immediately 
 *
 * Copyright 2004 Jochen Karrer. All rights reserved.
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
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/file.h>

#include "amdflash.h"
#include "configfile.h"
#include "cycletimer.h"
#include "bus.h"
#include "signode.h"
#include "diskimage.h"
#include "sgstring.h"


int verbosity = 1;

#if 0
#define dbgprintf(verb,x...) { if((verb)>=verbosity) fprintf(stderr,x); }
#else
#define dbgprintf(x...)
#endif

/*
 * ---------------------------------------------------------------------
 * Auto select mode: create a table with code/mask pairs
 * to decide which value has to returned
 * ---------------------------------------------------------------------
 */
#define MAX_AUTO_CODES (7)

typedef struct AutoSel {
	int code;
	int mask;
	int value;
} AutoSel;

typedef struct EraseRegion {
	uint32_t sectorsize;
	int sectors;
} EraseRegion;

typedef struct FlashType {
	char *name;
	int size;
	int min_sectorsize;
	uint16_t querystr[0x4c];	
	AutoSel auto_sel[MAX_AUTO_CODES];
	uint32_t capabilities;
} FlashType;

#define CAP_CFI 	(1)
#define CAP_TOPBOOT 	(2)
/* CMDSETS */
#define CAP_LOCK_REG	(4)
#define CAP_PASSWD	(8)
#define CAP_PPB		(0x10)
#define CAP_PPB_LOCK	(0x20)
#define CAP_DYB		(0x40)

typedef struct AMDFlashBank AMDFlashBank;

typedef struct AMD_Flash {
	AMDFlashBank *bank;
	uint8_t  *host_mem;	
	uint32_t  *statistics_mem;	
	FlashType *type;

	int n_regions;
	/* The erase region table is inverted in Initialization for Top Boot devices */
	EraseRegion erase_region[4];
	int rmap_size;
	uint32_t min_sectorsize;
	uint32_t rmap_mask;
	uint32_t rmap_shift;
	EraseRegion **rmap;

	/* State Variables */
	int mode;      /* current memory mapping mode (Read Array or read registers) */
	int cycle;     /* Number of current cycle in multiword cmd sequences */
	int cmd;       /* recognized command */
	int en_cmdset; /* Enabled cmdset (bypass Programm, Lock Register, PPB ... */
	
	/*
 	 * Write buffer
	 */
	uint32_t wb_sa; // write buffer sector address
	uint32_t wb_ba; // write buffer address 
	uint32_t wb_wc; // write buffer word count
	uint16_t wb[16];
	uint32_t wb_addr[16];
	uint32_t wb_valid[16];
	

	int cap_bufwrite; // max blocksize
	CycleTimer erase_timeout;
} AMD_Flash;

struct AMDFlashBank {
	BusDevice bdev;
	DiskImage *disk_image;
	DiskImage *stat_image;

	uint8_t  *host_mem;	
	uint32_t *statistics_mem;

	int nr_chips;
	int bankwidth; /* nr of bits */
	int addr_shift;
	int size;
	int endian;
	uint32_t endian_xor;
	SigNode *big_endianNode;
	SigTrace *big_endianTrace;
	AMD_Flash *flash[2];
};

/*
 * -----------------------------------------------------------------
 * Calculate Byteaddress from Memory Bus address (16 Bit mode)
 * And Register number from Byte-Address
 * -----------------------------------------------------------------
 */
#define MEMBUS_TO_FLASH_ADDR(bank,addr) ((addr)>>(bank)->addr_shift)
#define FLASH_ADDR_TO_MEMBUS(bank,addr) ((addr)<<(bank)->addr_shift)
#define BADDR(x) ((x)<<1)
#define FLASH_ADDR(x)	((x) >> 1)
#define FLASH_REG(x)	(((x) >> 1) & 0xfff)


#define MAP_READ_ARRAY	(0)
#define MAP_IO		(1)
#define MAP_CFI		(2)

/*
 * ----------------------------------------------------------------------------
 * State machine definitions for interpretation of incomming commands
 * The names are stolen from the Bus Cycle List of the am29lv640 documentation
 * The global state machine is left as soon as the command is identified,
 * and a command specific Sub statemachine is entered
 * ----------------------------------------------------------------------------
 */
#define STATE_START  (0)
#define STATE_FIRST  (1)
#define STATE_SECOND (2)
#define STATE_THIRD  (3)
#define STATE_FOURTH (4)
#define STATE_FIFTH  (5)
#define STATE_SIXTH  (6)

#define CYCLE_FIRST  (1)
#define CYCLE_SECOND (2)
#define CYCLE_THIRD  (3)
#define CYCLE_FOURTH (4)
#define CYCLE_FIFTH  (5)
#define CYCLE_SIXTH  (6)


#define CMD_NONE        	(0)
#define CMD_AUTOSELECT		(1)
#define CMD_ENTER_SECSI		(5)
#define CMD_EXIT_SECSI		(6)
#define CMD_PROGRAM		(7)
#define CMD_WRITE_TO_BUF	(8)
#define CMD_PGM_BUFFER		(9)
#define CMD_WBUF_ABORT_RESET	(10)
#define CMD_UNLOCK_BYPASS	(11)
#define CMD_UNLOCK_BYPASS_PGM   (12)
#define CMD_EXIT_CMDSET		(13)
#define CMD_ERASE		(14) 

/* Sector Protection commands from S29GL-P manual */
#define	CMD_ENTER_LOCK_REG	(15)
#define CMD_LOCKBITS_PGM	(16)
#define CMD_EXIT_LOCK_REG	(18)

#define CMD_ENTER_PW_PROT	(19)
#define CMD_PW_PGM	(20)
#define CMD_EXIT_PW_PROT	(23)

#define CMD_ENTER_PPB		(24)
#define	CMD_PPB_PGM		(25)
#define CMD_EXIT_PPB		(28)

#define CMD_ENTER_PPB_LOCK	(29)
#define CMD_PPB_LOCKBIT_SET	(30)
#define CMD_EXIT_PPB_LOCK	(32)

#define CMD_ENTER_DYB		(33)
#define CMD_DYB_SET		(34)
#define CMD_EXIT_DYB		(37)

/* The currently enabled cmdset */
#define ENCMDSET_NONE		(0)
#define ENCMDSET_BYPASS 	(1)
#define ENCMDSET_LOCK_REG	(2)
#define ENCMDSET_PW_PROT	(3)
#define ENCMDSET_PPB		(4)
#define ENCMDSET_PPB_LOCK	(5)
#define ENCMDSET_DYB		(6)


/*
 * ----------------------------------------------------------
 * Flash Types
 * 	Add new Flash Types here.
 *
 *	Warning: All AMD/Spansion documents for Non uniform 
 *	sector devices seem to contain wrong erase block 
 *	information. The Spansion documents seem to have
 *	all bugs from AMD with additional bugs introduced
 *	when rewriting. 
 *	Macronix is ok. 
 *
 * ----------------------------------------------------------
 */
static FlashType 
flash_types[] = {
	{
		name:	"AM29LV800BT",
		size:	    1*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { }, /* Not a CFI flash */
		auto_sel: { 
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x22da},	/* Device ID */
			{0x02,0x4f,0x0000},	/* SecSi lowest not locked */
		},
		capabilities: 0 
	},	
	{
		
		name:	"AM29LV160BB",
		size:	    2*1024*1024,
		min_sectorsize: 8*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 4, 
			0,0xa,0,5,0,4,0,0x15, 2,0,0,0,
			4, 
			0x00,0x00,0x40,0x00, 
			0x01,0x00,0x20,0x00, 
			0x00,0x00,0x80,0x00, 
			0x1e,0x00,0x00,0x01, 
			0,0,0,
			'P','R','I', 0x31,0x30,0,2,1,1,4,0,0,0,0x00,0x00,0
			,0
		},
		auto_sel: { 
			{0x00,0x43,0x0001},	/* Manufacturer ID */
			{0x01,0x43,0x2249},	/* Device ID */
			{0x02,0x43,0x0000},	/* Sector Protect verify */
		},
		capabilities: CAP_CFI
	},	
	{
		
		name:	"AM29LV160BT",
		size:	    2*1024*1024,
		min_sectorsize: 8*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 4, 
			0,0xa,0,5,0,4,0,0x15, 2,0,0,0,
			4, 
			0x00,0x00,0x40,0x00, 
			0x01,0x00,0x20,0x00, 
			0x00,0x00,0x80,0x00, 
			0x1e,0x00,0x00,0x01, 
			0,0,0,
			'P','R','I', 0x31,0x30,0,2,1,1,4,0,0,0,0x00,0x00,0
			,0
		},
		auto_sel: { 
			{0x00,0x43,0x0001},	/* Manufacturer ID */
			{0x01,0x43,0x22c4},	/* Device ID */
			{0x02,0x43,0x0000},	/* Sector Protect verify */
		},
		capabilities: CAP_CFI  | CAP_TOPBOOT
	},	
	{
		
		name:	"AM29LV320MB",
		size:	    4*1024*1024,
		min_sectorsize: 8*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x16, 2,0,5,0,
			2, 
			0x07,0x00,0x20,0x00,  /* Docu says 0x7f 0x00 0x20 0x00 (Buggy) */ 
			0x3e,0x00,0x00,0x01, 
			0x00,0x00,0x00,0x00, 
			0x00,0x00,0x00,0x00, 
			0,0,0,
			'P','R','I', 0x31,0x33,8,2,1,1,4,0,0,1,0xb5,0xc5,2
			,1
		},
		auto_sel: { 
			{0x0e,0x4f,0x221a},	/* Ex Device ID */
			{0x0f,0x4f,0x2200},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		/* ST Mircoelectronics document 7876 */	
		name:	"M29W320DB",
		size:	    4*1024*1024,
		min_sectorsize: 8*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0xb5,0xc5, 4, 
			0,0xa,0,5,0,4,0,0x16, 2,0,0,0,
			4, 
			0x00,0x00,0x40,0x00,  /* 1 * 16k  */ 
			0x01,0x00,0x20,0x00,  /* 2 * 8k   */
			0x00,0x00,0x80,0x00,  /* 1 * 32k  */
			0x3e,0x00,0x00,0x01,  /* 63 * 64k */
			0,0,0,
			'P','R','I', 0x31,0x30,0,2,1,1,4,0,0,0,0xb5,0xc5,2
			
		},
		auto_sel: { 
			{0x00,0x03,0x0020},	/* Manufacturer ID */
			{0x01,0x03,0x22cb},	/* Device ID */
		},
		capabilities: CAP_CFI
	},	
	{
		
		name:	"MX29LV800CB",
		size:	    1*1024*1024,
		min_sectorsize: 8*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 4, 
			0,0xa,0,5,0,4,0,0x14, 2,0,0,0,
			4,
			0x00,0x00,0x40,0x00, /* 1  * 16k */
			0x01,0x00,0x20,0x00, /* 2  * 8k  */
			0x00,0x00,0x80,0x00, /* 1  * 32k */
			0x0e,0x00,0x00,0x01, /* 15 * 64k */
			0,0,0,
			'P','R','I', 0x31,0x30,0,2,1,1,4,0,0,0,0,0,0
			,0
		},
		auto_sel: { 
			{0x00,0x43,0x00c2},	/* Manufacturer ID */
			{0x01,0x43,0x225b},	/* Device ID */
			{0x02,0x43,0x0000},	/* Sector unprotected */
		},
		capabilities: CAP_CFI
	},	
	{
		
		name:	"MX29LV800CT",
		size:	    1*1024*1024,
		min_sectorsize: 8*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 4, 
			0,0xa,0,5,0,4,0,0x14, 2,0,0,0,
			4,
			0x00,0x00,0x40,0x00, /* 1  * 16k */
			0x01,0x00,0x20,0x00, /* 2  * 8k  */
			0x00,0x00,0x80,0x00, /* 1  * 32k */
			0x0e,0x00,0x00,0x01, /* 15 * 64k */
			0,0,0,
			'P','R','I', 0x31,0x30,0,2,1,1,4,0,0,0,0,0,0
			,0
		},
		auto_sel: { 
			{0x00,0x43,0x00c2},	/* Manufacturer ID */
			{0x01,0x43,0x22da},	/* Device ID */
			{0x02,0x43,0x0000},	/* Sector unprotected */
		},
		capabilities: CAP_CFI  | CAP_TOPBOOT
	},	
	/* Same as S29GL032M */
	{
		name:	"AM29LV320ML",
		size:	    4*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x16, 2,0,5,0,
			1, 
			0x3f,0x00,0x00,0x01, /* 64 * 64k */ 
			0x00,0x00,0x00,0x00, 
			0x00,0x00,0x00,0x00, 
			0x00,0x00,0x00,0x00, 
			0,0,0,
			'P','R','I', 0x31,0x33,8,2,1,1,4,0,0,1,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x0e,0x4f,0x221d},	/* Ex Device ID */
			{0x0f,0x4f,0x2200},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	/* Same as S29GL064A */
	{
		name:	"AM29LV640ML",
		size:	    8*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x17, 2,0,5,0,1, 0x7f,0,0,
			1, 0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,0,2,1,1,4,0,0,1,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x0e,0x4f,0x220c},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		name:	"MBM29LV650UE",
		size:	    8*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 4, 
			0,0xa,0,5,0,4,0,0x17, 1,0,0,0,1, 0x7f,0,0,
			1, 0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x31,1,2,4,1,4,0,0,0,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x00,0x4f,0x0004},	/* Manufacturer ID */
			{0x01,0x4f,0x22d7},	/* Device ID */
			{0x02,0x4f,0x0000},	/* Sector Group Protection */
			{0x03,0x4f,0x0010},	/* Extended ID */
		},
		capabilities: CAP_CFI
	},	
	{
		name:	"AM29LV128ML",
		size:	    16*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x18, 2,0,5,0,1, 0xff,0,0,
			1, 0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,8,2,1,1,4,0,0,1,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
			{0x0e,0x4f,0x2212},	/* Ex Device ID */
			{0x0f,0x4f,0x2200},	/* Ex2 Device ID */
		},
		capabilities: CAP_CFI
	},	
	{
		name:	"AM29LV256ML",
		size:	    32*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x19, 2,0,5,0,1, 0xff,1,0,
			1, 0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,8,2,1,1,4,0,0,1,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x0e,0x4f,0x2212},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		/* No differences found to AM29LV256ML */
		name:	"S29GL256MR2",
		size:	    32*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x19, 2,0,5,0,1, 0xff,1,0,1, 
				0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,8,2,1,1,4,0,0,1,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x0e,0x4f,0x2212},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		/* bottom boot (non Uniform) sector version does not really exist ! */
		name:	"S29GL256MR4",
		size:	    32*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x19, 2,0,5,0,
			2, 
			0x07,0x00,0x20,0x00,	/* 8 * 8k 	*/
			0xfe,0x01,0x00,0x01, 	/* 511 * 64k	*/
			0x00,0x00,0x00,0x00,
			0x00,0x00,0x00,0x00,
			0,0,0,
			'P','R','I', 0x31,0x33,8,2,1,1,4,0,0,1,0xb5,0xc5,2
			,1
		},
		/* Autoselect codes are fantasy because the Device does not exist */
		auto_sel: { 
			{0x0e,0x4f,0x2212},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		/* Warning ! Multiple independent banks not supported */
		name: "AM29BDS128H",
		size: 16*1024*1024,
		min_sectorsize: 8*1024,
		querystr: { 
			'Q','R','Y',0x02,0,0x40,0,0,0,0,0,0x17,0x19,0,0,4,
			0,9,0,4,0 ,4,0,  0x18,1,0,0,0,3, 
			7,0,0x20,0,  /* 8 * 8k    */
			0xfd,0,0,1,  /* 254 * 64k */ 
			7,0,0x20,0,  /* 8 * 8k    */
			0,0,0,0, 
			0,0,0,
			'P','R','I',0x31,0x33,0x0c,0x02,0x01,0,0x07,0xe7,1,0,0xb5,0xc5, 1
			,0,0,0,0,0,0,0,
			4,0x27,0x60,0x60,0x27	
		},
		auto_sel: {
			{0x00,0x4f,0x0001},
			{0x01,0x7f,0x227E},
			{0x0e,0x7f,0x2218},
			{0x0f,0x7f,0x2200},
		},
		capabilities: CAP_CFI
	},
	{
		name:	"S29GL128NR2",
		size:	    16*1024*1024,
		min_sectorsize: 128*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x18, 2,0,5,0,1, 0x7f,0,0,2, 
				0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,0x10,2,1,0,8,0,0,2,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x0e,0x4f,0x2221},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		name:	"S29GL256NR2",
		size:	    32*1024*1024,
		min_sectorsize: 128*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x19, 2,0,5,0,1, 0xff,0,0,2, 
				0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,0x10,2,1,0,8,0,0,2,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x0e,0x4f,0x2222},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		name:	"S29GL512NR2",
		size:	    64*1024*1024,
		min_sectorsize: 128*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 7, 
			7,0xa,0,1,5,4,0,0x1a, 2,0,5,0,1, 0xff,1,0,2, 
				0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,0x10,2,1,0,8,0,0,2,0xb5,0xc5,4
			,1
		},
		auto_sel: { 
			{0x0e,0x4f,0x2223},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x03,0x4f,0x0008},	/* SecSi lowest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		name:	"S29GL128GP11TFI010",
		size:	    16*1024*1024,
		min_sectorsize: 128*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0,6, 
			6,9,0x13,3,5,3,2,0x18, 2,0,6,0,1, 0x7f,0,0,2, 
				0,0,0,0, 0,0,0,0, 0,0,0,0,  0xffff,0xffff,0xffff,
			'P','R','I', 0x31,0x33,0x14,2,1,0,8,0,0,2,0xb5,0xc5,5
			,1
		},
		auto_sel: { 
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x0e,0x4f,0x2221},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x02,0x4f,0x0000},	/* Sector group Prot. verification */
			{0x03,0x4f,0x0019},	/* SecSi highest not locked */
		},
		capabilities: CAP_CFI
	},	
	{
		name:	"S29GL256GP11TFI010",
		size:	    32*1024*1024,
		min_sectorsize: 128*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0,6, 
			6,9,0x13,3,5,3,2,0x19, 2,0,6,0,1, 0xff,0,0,2, 
				0,0,0,0, 0,0,0,0, 0,0,0,0,  0xffff,0xffff,0xffff,
			'P','R','I', 0x31,0x33,0x14,2,1,0,8,0,0,2,0xb5,0xc5,5
			,1
		},
		auto_sel: { 
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x0e,0x4f,0x2222},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x02,0x4f,0x0000},	/* Sector group Prot. verification */
			{0x03,0x4f,0x0019},	/* SecSi highest not locked */
		},
		capabilities: CAP_CFI | CAP_TOPBOOT
	},	
	{
		name:	"S29GL512GP11TFI010",
		size:	    64*1024*1024,
		min_sectorsize: 128*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0,6, 
			6,9,0x13,3,5,3,2,0x1a, 2,0,6,0,1, 0xff,1,0,2, 
				0,0,0,0, 0,0,0,0, 0,0,0,0,  0xffff,0xffff,0xffff,
			'P','R','I', 0x31,0x33,0x14,2,1,0,8,0,0,2,0xb5,0xc5,5
			,1
		},
		auto_sel: { 
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x0e,0x4f,0x2223},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x02,0x4f,0x0000},	/* Sector group Prot. verification */
			{0x03,0x4f,0x0019},	/* SecSi highest not locked */
		},
		capabilities: CAP_CFI | CAP_TOPBOOT
	},	
	{
		name:	"S29GL01GP11TFI010",
		size:	    128*1024*1024,
		min_sectorsize: 128*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0,6, 
			6,9,0x13,3,5,3,2,0x1b, 2,0,6,0,1, 0xff,3,0,2, 
				0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,0x14,2,1,0,8,0,0,2,0xb5,0xc5,5
			,1
		},
		auto_sel: { 
			{0x00,0x4f,0x0001},	/* Manufacturer ID */
			{0x01,0x4f,0x227e},	/* Device ID */
			{0x0e,0x4f,0x2228},	/* Ex Device ID */
			{0x0f,0x4f,0x2201},	/* Ex2 Device ID */
			{0x02,0x4f,0x0000},	/* Sector group Prot. verification */
			{0x03,0x4f,0x0019},	/* SecSi highest not locked */
		},
		capabilities: CAP_CFI | CAP_TOPBOOT
	},	
	{
		name:	"MX29LV640BU",
		size:	    8*1024*1024,
		min_sectorsize: 64*1024,	
		querystr: { 'Q','R','Y', 0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0, 4, 
			0,0xa,0,5,0,4,0,0x17, 1,0,0,0,1, 0x7f,0,0,
			1, 0,0,0,0, 0,0,0,0, 0,0,0,0,  0,0,0,
			'P','R','I', 0x31,0x33,0,2,4,1,4,0,0,0,0xb5,0xc5,0
			,1
		},
		auto_sel: { 
			{0x00,0x4f,0x00c2},	/* Manufacturer ID */
			{0x01,0x4f,0x22d7},	/* Device ID */
			{0x02,0x4f,0x0000},	/* Sector Group protect */
			{0x03,0x4f,0x0008},	/* lowest not factory locked */
		},
		capabilities: CAP_CFI
	},	
	{
		name: "S29AL016DT", /* Name in datasheet extended by top boot "T" */
		size: 2*1024*1024,
		min_sectorsize: 8*1024,
		querystr: { 
			'Q','R','Y',0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0,4,
			0,0xa,0,5,0 ,4,0,  0x15,2,0,0,0,4, 
			0,0,0x40,0,  /* 1 * 16k    */
			1,0,0x20,0,  /* 2 * 8k     */
			0,0,0x80,0,  /* 1 * 32k    */
			0x1e,0,0,1,  /* 31 * 64k  */
			0,0,0,
			'P','R','I',0x31,0x30,0x00,0x02,0x01,1,0x04,0,0,0,
		},
		auto_sel: {
			{0x00,0x4f,0x0001},
			{0x01,0x4f,0x22C4},
		},
		capabilities: CAP_CFI
	},
	{
		name: "S29AL016DB", /* Name in datasheet extended by bootom boot "B" */
		size: 2*1024*1024,
		min_sectorsize: 8*1024,
		querystr: { 
			'Q','R','Y',0x02,0,0x40,0,0,0,0,0,0x27,0x36,0,0,4,
			0,0xa,0,5,0 ,4,0,  0x15,2,0,0,0,4, 
			0,0,0x40,0,  /* 1 * 16k    */
			1,0,0x20,0,  /* 2 * 8k     */
			0,0,0x80,0,  /* 1 * 32k    */
			0x1e,0,0,1,  /* 31 * 64k  */
			0,0,0,
			'P','R','I',0x31,0x30,0x00,0x02,0x01,1,0x04,0,0,0,
		},
		auto_sel: {
			{0x00,0x4f,0x0001},
			{0x01,0x4f,0x2249},
		},
		capabilities: CAP_CFI
	},
}; 

/*
 * -----------------------------------------
 * Read a cfi register from flash type
 * -----------------------------------------
 */
static inline uint16_t 
cfi_read(FlashType *ftype,int reg) {
	if(reg >= 0x10 && reg <= 0x5b) {
		return ftype->querystr[reg-0x10];
	} else {
		return 0;
	}
}

static inline int 
get_sectorsize(AMD_Flash *flash,uint32_t byte_addr) {
	EraseRegion *region;
	int index = ((byte_addr) & flash->rmap_mask) >> flash->rmap_shift;
	region = flash->rmap[index];
	return region->sectorsize;
}

static inline int
address_to_sa(AMD_Flash *flash,uint32_t byte_addr) {
	return byte_addr & ~(get_sectorsize(flash,byte_addr)-1);
}

static void 
switch_to_readarray(AMD_Flash *flash) {
	if(flash->mode!=MAP_READ_ARRAY) {
		flash->mode=MAP_READ_ARRAY;
		Mem_AreaUpdateMappings(&flash->bank->bdev);
	}
}

static void 
switch_to_iomode(AMD_Flash *flash) {
	if(flash->mode!=MAP_IO) {
		flash->mode=MAP_IO;
		Mem_AreaUpdateMappings(&flash->bank->bdev);
	}
}

static void 
switch_to_cfimode(AMD_Flash *flash) {
	if(flash->mode!=MAP_CFI) {
		flash->mode=MAP_CFI;
		Mem_AreaUpdateMappings(&flash->bank->bdev);
	}
}

/*
 * ------------------------------------------------------------------------------
 * The Big State machine
 * 	Action of flash is dependent on the number of the Cycle (from the command
 * 	definition Table in the AMD Manual) and the Command possibly recognized
 * 	in an earlier Cycle.   
 *	the variables flash->cycle and flash->cmd are used to store the state
 * -------------------------------------------------------------------------------
 */
static void 
first_cycle_write(AMD_Flash *flash,uint16_t value,uint32_t dev_addr) {
	uint32_t reg = FLASH_REG(dev_addr);
//	fprintf(stderr,"%s: value %08x, register %08x\n",flash->type->name,value,reg); 
	if(flash->cmd != CMD_NONE) {
		fprintf(stderr,"CMD not NONE in first cycle\n");
		return;
	}
	if((reg==0x555) && (value == 0xaa)) {
		flash->cycle++;
		return;
	} 
	/* Check if this shouldn't work in all cycles ????? */
	if((value==0xf0) || (value == 0xff)) {
		// reset
		switch_to_readarray(flash);
		return;
	}
	if((value==0x98) && ((reg==0x55) | (reg == 0x555))) {
		if(flash->type->capabilities & CAP_CFI) {
			switch_to_cfimode(flash);
		}
		return;
	} 
	if((value == 0xa0))  {
		switch(flash->en_cmdset) {
			case ENCMDSET_NONE:
				break;

			case ENCMDSET_BYPASS:
				flash->cycle++;
				flash->cmd = CMD_PROGRAM;
				break;

			case ENCMDSET_LOCK_REG:
				flash->cycle++;
				flash->cmd =  CMD_LOCKBITS_PGM;
				break;

			case ENCMDSET_PW_PROT:
				flash->cycle++;
				flash->cmd =  CMD_PW_PGM;
				break;

			case ENCMDSET_PPB:
				flash->cycle++;
				flash->cmd = CMD_PPB_PGM;
				break;

			case ENCMDSET_PPB_LOCK:
				flash->cycle++;
				flash->cmd = CMD_PPB_LOCKBIT_SET;
				break;

			case ENCMDSET_DYB:	
				flash->cycle++;
				flash->cmd = CMD_PPB_LOCKBIT_SET;
				break;

			default:
				fprintf(stderr,"AMDFLASH: illegal cmdset selection\n");
		}
	}
	if((value == 0x90) &&  (flash->en_cmdset==ENCMDSET_BYPASS)) {
		flash->cycle++;
		flash->cmd = CMD_EXIT_CMDSET;
		return;
	}
	if((value == 0xb0)) {
		static int maxmsgs = 5;
		// ignore suspend resume	
		if(maxmsgs > 0) {
			fprintf(stderr,"AMD_Flash: Got suspend, but not busy !\n");
			maxmsgs--;
		}
		return;
	} 
	if((value == 0x30)) {
		static int maxmsgs = 5;
		// ignore suspend resume	
		if(maxmsgs > 0) {
			fprintf(stderr,"AMD_Flash: Ignore resume because not suspended\n");
			maxmsgs--;
		}
		return;
	} 
	{
		static int flag=0;
		if(!flag) {
			flag=1;
			fprintf(stderr,"%s: Unknown first cycle value %08x, register %08x\n",flash->type->name,value,reg);
		}
	}
	return;
}

/*
 * -------------------------------------------
 * Write a 16 Bit word to the flash 
 * Bits can only be cleared. 
 * -------------------------------------------
 */
static inline void
programm_word(AMD_Flash *flash,uint16_t value,uint32_t dev_addr) {
	uint16_t *dst = (uint16_t*)(flash->host_mem+FLASH_ADDR_TO_MEMBUS(flash->bank,dev_addr&~1UL));	
	*dst=value & *dst;
}

static void 
second_cycle_write(AMD_Flash *flash,uint16_t value,uint32_t dev_addr) {
	uint32_t reg = FLASH_REG(dev_addr);
	if(flash->cmd == CMD_NONE) {
		if((reg==0x2aa) && (value==0x55)) {
			flash->cycle++;
			return;
		} else if(((reg & 0xff) ==  0) && (value == 0x70)) {
			/* ignore sector unlocks */
			flash->cycle = CYCLE_FIRST;
			flash->cmd=CMD_NONE;
			return;
		}
		fprintf(stderr,"Unknown: value %08x to reg %08x in second cycle\n",value,reg);
		return;
	} else if(flash->cmd==CMD_PROGRAM) {
		programm_word(flash,value,dev_addr);
		flash->cycle=CYCLE_FIRST;
		flash->cmd=CMD_NONE;
		return;
	} else if(flash->cmd == CMD_EXIT_CMDSET) {
		flash->en_cmdset=ENCMDSET_NONE;
		flash->cmd=CMD_NONE;	
		flash->cycle=CYCLE_FIRST;
		return;
	} else {
		fprintf(stderr,"Unknown command %d in second cycle\n",flash->cmd);
		flash->cmd=CMD_NONE;	
		flash->cycle=CYCLE_FIRST;
		return;
	} 
}
static void 
third_cycle_write(AMD_Flash *flash,uint16_t value,uint32_t dev_addr) {
	uint32_t reg = FLASH_REG(dev_addr);
	uint32_t cap = flash->type->capabilities;
	if(flash->cmd != CMD_NONE) {
		fprintf(stderr,"Unknown cmd %d in third cycle\n",flash->cmd);
		return;
	}
	/* Sector Address */
	if((value==0x25)) {
		if(flash->cap_bufwrite>1) {
			flash->wb_sa=address_to_sa(flash,dev_addr);
			flash->cmd=CMD_WRITE_TO_BUF;
			flash->cycle++;
		} else {
			flash->cmd = CMD_NONE;
			flash->cycle=CYCLE_FIRST;	
			fprintf(stderr,"%s: flash type doesn't support Buffer write\n",flash->type->name);
		}
		return;
	} 
	if(reg!=0x555) {
		fprintf(stderr,"Wrong reg %08x, value %08x in third cycle\n",value,reg);
		flash->cmd = CMD_NONE;
		flash->cycle=CYCLE_FIRST;	
		return;
	}
	switch(value) {
		case 0x90:
			flash->cmd=CMD_AUTOSELECT;
			switch_to_iomode(flash); 
			flash->cycle++;	
			break;
		case 0x88:
			fprintf(stderr,"Enter SECSI not implemented\n");
			flash->cmd = CMD_NONE;
			flash->cycle=CYCLE_FIRST;	
			break;

		case 0xa0:
			flash->cmd = CMD_PROGRAM;
			flash->cycle++;
			break;

		/* Write to buffer abort */
		case 0xf0:
			flash->cmd = CMD_NONE;
			flash->cycle=CYCLE_FIRST;
			break;

		/* Unlock Bypass */
		case 0x20:
			flash->en_cmdset=ENCMDSET_BYPASS;
			flash->cmd=CMD_NONE;
			flash->cycle=CYCLE_FIRST;
			break;

		case 0x40:
			if(cap & CAP_LOCK_REG) {
				flash->en_cmdset=ENCMDSET_LOCK_REG;
				flash->cmd=CMD_NONE;
				flash->cycle=CYCLE_FIRST;
			} 
			break;

		case 0x50:
			if(cap & CAP_PPB_LOCK) {
				flash->en_cmdset=ENCMDSET_PPB_LOCK;
				flash->cmd=CMD_NONE;
				flash->cycle=CYCLE_FIRST;
			} 
			break;

		case 0x60:
			if(cap & CAP_PASSWD) {
				flash->en_cmdset=ENCMDSET_PW_PROT;
				flash->cmd=CMD_NONE;
				flash->cycle=CYCLE_FIRST;
			} 
			break;

		case 0x80:
			flash->cmd=CMD_ERASE;
			flash->cycle++;
			break;

		case 0xC0:
			if(cap & CAP_PPB) {
				flash->en_cmdset=ENCMDSET_PPB;
				flash->cmd=CMD_NONE;
				flash->cycle=CYCLE_FIRST;
			} 
			break;

		case 0xE0:
			if(cap & CAP_DYB) {
				flash->en_cmdset=ENCMDSET_DYB;
				flash->cmd=CMD_NONE;
				flash->cycle=CYCLE_FIRST;
			} 
			break;
			
		default:
			fprintf(stderr,"unknown value %04x in third cycle\n",value);
	}
	return;
}


static void 
fourth_cycle_write(AMD_Flash *flash,uint16_t value,uint32_t dev_addr) 
{
	uint32_t reg = FLASH_REG(dev_addr);
	uint32_t cmd=flash->cmd;
	if(cmd==CMD_AUTOSELECT) {
		if((value==0xf0) || (value == 0xff)) {
			switch_to_readarray(flash);
		} else if(value==0) {
			fprintf(stderr,"Leave Sec SI region\n");
		} else {
			fprintf(stderr,"%s: Wrong value %04x in leave SEC-SI\n",flash->type->name,value);
		}
		flash->cmd=CMD_NONE;
		flash->cycle=CYCLE_FIRST;
		return;
	} else if(cmd==CMD_PROGRAM) {
		programm_word(flash,value,dev_addr);
		flash->cycle=CYCLE_FIRST;
		flash->cmd=CMD_NONE;
		return;
	} else if(cmd==CMD_WRITE_TO_BUF)  {
		if(flash->wb_sa!=address_to_sa(flash,dev_addr)) {
			fprintf(stderr,"Sector Address changed from 0x%08x to 0x%08x"
			" during write to buffer\n",flash->wb_sa,dev_addr); 
			flash->cycle=CYCLE_FIRST;
			flash->cmd=CMD_NONE;
			return;
		}
		flash->wb_wc=value & 0xff;
		flash->cycle++;
		return;
	} else if(cmd==CMD_ERASE) {
		if((reg==0x555) && (value == 0xaa)) {
			flash->cycle++;	
			return;
		} else {
			fprintf(stderr,"Error in Erase sequence reg %08x value %04x: Aborting Erase\n",reg,value);
			flash->cmd=CMD_NONE;
			flash->cycle=CYCLE_FIRST;
			return;
		}
	} else {
		fprintf(stderr,"unknown cmd %d in cycle four\n",flash->cycle); 
		flash->cmd=CMD_NONE;
		flash->cycle=CYCLE_FIRST;
		return;
	}
}
static void 
fifth_cycle_write(AMD_Flash *flash,uint16_t value,uint32_t dev_addr) 
{
	uint32_t reg = FLASH_REG(dev_addr);
	int cmd = flash->cmd;
	if(cmd==CMD_WRITE_TO_BUF) {
		int i;
		int buf_index;
		for(i=0;i<16;i++) {
			flash->wb_valid[i]=0;
		}
		if(address_to_sa(flash,dev_addr) != flash->wb_sa) {
			fprintf(stderr,"fifth: sector address changed during buffer write"
				" dev_addr %08x, wb_sa %08x\n",dev_addr,flash->wb_sa);
			flash->cycle=CYCLE_FIRST;
			flash->cmd=CMD_NONE;
			return;
		}
		buf_index = reg & 0xf;
		flash->wb[buf_index] = value & 0xffff;
		flash->wb_addr[buf_index] = FLASH_ADDR(dev_addr);
		flash->wb_valid[buf_index] = 1;
		flash->wb_ba = FLASH_ADDR(dev_addr) & ~UINT32_C(0xf);
		flash->cycle++;
		if(flash->wb_wc==0) { // do not decrement
			//fprintf(stderr,"WB Complete\n");
			flash->cmd=CMD_PGM_BUFFER;
			flash->cycle++;
			return;
		}
		return;
	} else if (cmd==CMD_ERASE) {
		if((reg==0x2aa) && (value==0x55)) {
			flash->cycle++;
			return;
		} else {
			fprintf(stderr,"Aborting erase sequence in fifth cycle reg %08x,value %04x\n",reg,value);
			flash->cycle=CYCLE_FIRST;
			flash->cmd=CMD_NONE;
			return;
		}
	} else {
		fprintf(stderr,"Unknown command %d in fifth cycle\n",cmd);
		flash->cycle=CYCLE_FIRST;
		flash->cmd=CMD_NONE;
		return;
	}	
}

static int
pgm_buffer(AMD_Flash *flash) 
{
	int i;
	for(i=0;i<16;i++) {
		uint32_t addr=BADDR(flash->wb_addr[i]);	
		if(!flash->wb_valid[i]) {
			continue;	
		}
		programm_word(flash,flash->wb[i],addr);
	}
	return 0;
}

/*
 * --------------------------------------
 * Erase a sector
 * sa is the Start address of the sector
 * --------------------------------------
 */
static void
erase_sector(AMD_Flash *flash,uint32_t sa) {
	AMDFlashBank *bank = flash->bank;
	int i;
	int sectorsize=get_sectorsize(flash,sa);
	if(flash->statistics_mem) {
		int sector=sa/flash->min_sectorsize;
		/* Misuse of translation Macro */
		flash->statistics_mem[FLASH_ADDR_TO_MEMBUS(bank,sector)]++;
	}
	dbgprintf("erase sector at 0x%08x, size %d\n",sa,sectorsize);
	for(i=0;i<sectorsize;i+=2) {
		uint32_t addr = FLASH_ADDR_TO_MEMBUS(bank,i+sa);
		*(uint16_t*)(flash->host_mem+addr)=0xffff;
	}
}

/*
 * ----------------------------------------------------
 * After erase cylce you can sent next erase command
 * without unlock sequence within 50 usec 
 * ----------------------------------------------------
 */
static void
erase_timeout(void *cd) {
	AMD_Flash *flash=cd;
	if((flash->cmd==CMD_ERASE)&&(flash->cycle==CYCLE_SIXTH)) {
		flash->cycle=CYCLE_FIRST;
		flash->cmd=CMD_NONE;
	} else {
		fprintf(stderr,"erase_timeout Bug\n");
	}
}

static void 
sixth_cycle_write(AMD_Flash *flash,uint16_t value,uint32_t dev_addr) 
{
	uint32_t reg = FLASH_REG(dev_addr);
	int cmd = flash->cmd;
	if(cmd==CMD_WRITE_TO_BUF) {
		int buf_index;
		if((FLASH_ADDR(dev_addr) & ~UINT32_C(0xf)) != flash->wb_ba) {
			fprintf(stderr,"outside of writebuffer, addr %08x ba %08x\n",dev_addr,flash->wb_ba);
			flash->cycle=CYCLE_FIRST;		
			flash->cmd=CMD_NONE;
			return;
		} 
		buf_index = reg & 0xf;
		flash->wb[buf_index] = value & 0xffff;
		flash->wb_addr[buf_index] = FLASH_ADDR(dev_addr);
		flash->wb_valid[buf_index] = 1;
		if(--flash->wb_wc==0) {
			//fprintf(stderr,"WB Complete\n");
			flash->cmd=CMD_PGM_BUFFER;
			flash->cycle++;
			return;
		} else {
		//	fprintf(stderr,"WB Remaining %d\n",flash->wb_wc);
		}
		flash->cycle++;
		return;
	} else if (cmd==CMD_ERASE) {
		if((reg==0x555) && (value==0x10)) {
			uint32_t sa;
			for(sa=0;sa<flash->type->size;sa+=get_sectorsize(flash,sa)) {
				erase_sector(flash,sa);
			}	
			flash->cycle=CYCLE_FIRST;
			flash->cmd=CMD_NONE;
			return;
		} else if(value==0x30) {
			uint32_t sa = address_to_sa(flash,dev_addr);
			erase_sector(flash,sa);
			CycleTimer_Mod(&flash->erase_timeout,MicrosecondsToCycles(50));
			return;
		} else {
			CycleTimer_Remove(&flash->erase_timeout);
			flash->cycle=CYCLE_FIRST;
			flash->cmd=CMD_NONE;
			return;
		}
	} else if (cmd==CMD_PGM_BUFFER) {
		if((value&0xff)==0x29) {
			uint32_t sa=address_to_sa(flash,dev_addr);
			if(sa!= flash->wb_sa) {
				fprintf(stderr,"Wrong Sector Address %08x instead %08x\n",sa,flash->wb_sa);
			} else {
				pgm_buffer(flash);
			}
		} else {
			fprintf(stderr,"Wrong magic value in Write to buffer command\n");
		}
		flash->cmd=CMD_NONE;
		flash->cycle=CYCLE_FIRST;
		return;
	} else {
		fprintf(stderr,"Unknown cmd %d in sixth cycle\n",cmd);
		flash->cycle=CYCLE_FIRST;
		flash->cmd=CMD_NONE;
		return;
	}	
}

/*
 * -------------------------------------------------------------
 * n'th cycle write 
 * Cycle number > 6 only reached for buffer programming sequence
 * -------------------------------------------------------------
 */

static void 
nth_cycle_write(AMD_Flash *flash,uint16_t value,uint32_t dev_addr) 
{
	uint32_t reg = FLASH_REG(dev_addr);
	int cmd = flash->cmd;
	if(cmd==CMD_WRITE_TO_BUF) {
		int buf_index;
		if((FLASH_ADDR(dev_addr) & ~UINT32_C(0xf)) != flash->wb_ba) {
			fprintf(stderr,"outside of writebuffer, addr %08x ba %08x\n",dev_addr,flash->wb_ba);
			flash->cycle=CYCLE_FIRST;		
			flash->cmd=CMD_NONE;
			return;
		} 
		buf_index = reg & 0xf;
		flash->wb[buf_index] = value & 0xffff;
		flash->wb_addr[buf_index] = FLASH_ADDR(dev_addr);
		flash->wb_valid[buf_index]=1;
		if(--flash->wb_wc==0) {
			//fprintf(stderr,"WB Complete\n");
			flash->cmd=CMD_PGM_BUFFER;
			flash->cycle++;
			return;
		} else {
		//	fprintf(stderr,"WB Remaining %d\n",flash->wb_wc);
		}
		flash->cycle++;
		return;
	} else if (cmd==CMD_PGM_BUFFER) {
		flash->cmd=CMD_NONE;
		flash->cycle=CYCLE_FIRST;
		if(value==0x29) {
			uint32_t sa=address_to_sa(flash,dev_addr);
			if(sa!= flash->wb_sa) {
				fprintf(stderr,"Wrong Sector Address %08x instead %08x\n",sa,flash->wb_sa);
			} else {
				pgm_buffer(flash);
			}
			return;
		} else {
			fprintf(stderr,"Wrong value %04x in PGM Buffer Command\n",value);
			return;
		}
	} else {
		fprintf(stderr,"Unknown cmd %d in cycle %d\n",cmd,flash->cycle);
		flash->cycle=CYCLE_FIRST;
		flash->cmd=CMD_NONE;
		return;
	}	
}

static void 
flash_write(void *clientData,uint16_t value,uint32_t address,int rqlen) 
{
	AMD_Flash *flash=clientData;
	uint32_t dev_addr=address & (flash->type->size-1);
	//fprintf(stderr,"flash write value %08x address %08x cycle %d\n",value,address,flash->cycle); 
	if(flash->cycle == CYCLE_FIRST) {
		return first_cycle_write(flash,value,dev_addr);
	} else if(flash->cycle == CYCLE_SECOND) {
		return second_cycle_write(flash,value,dev_addr);
	} else if(flash->cycle == CYCLE_THIRD) {
		return third_cycle_write(flash,value,dev_addr);
	} else if(flash->cycle == CYCLE_FOURTH) {
		return fourth_cycle_write(flash,value,dev_addr);
	} else if(flash->cycle == CYCLE_FIFTH) {
		return fifth_cycle_write(flash,value,dev_addr);
	} else if(flash->cycle == CYCLE_SIXTH) {
		return sixth_cycle_write(flash,value,dev_addr);
	} else if(flash->cycle > CYCLE_SIXTH ) {
		return nth_cycle_write(flash,value,dev_addr);
	} else {
		fprintf(stderr,"write in cycle %d\n",flash->cycle);
		return;
	}
}

/*
* -----------------------------------------------------------
* Distribute a write access of 8/16 or 32 Bit to 
* the flash chips of a Flash Bank
* -----------------------------------------------------------
*/
static void
flashbank_write(void *clientData,uint32_t value, uint32_t mem_addr,int rqlen) {
	AMDFlashBank *bank=clientData;
	uint32_t bank_addr = mem_addr & (bank->size-1);
	int chip,shift;
	unsigned int flash_addr = MEMBUS_TO_FLASH_ADDR(bank,bank_addr);
	unsigned int first_chip = (bank_addr>>1) & (bank->nr_chips-1);
	shift=0;
	chip=first_chip;
	while(rqlen>0) {
		if(rqlen>1) {
			flash_write(bank->flash[chip],value>>shift,flash_addr ^ bank->endian_xor,2);
			rqlen-=2;
			chip = (chip+1) & (bank->nr_chips-1);
			if(!chip) {
				flash_addr+=2;
			}
			shift+=16;
		} else {
			flash_write(bank->flash[chip],value>>shift,flash_addr ^ bank->endian_xor,1);
			rqlen--;
		}
	}
}

static uint16_t 
first_cycle_read(AMD_Flash *flash,uint32_t address) 
{
//	fprintf(stderr,"Read in map mode\n");
	return 0;
}
static uint16_t 
fourth_cycle_read(AMD_Flash *flash,uint32_t dev_addr) 
{
	int i;
	uint32_t value=0;
	FlashType *ftype = flash->type;
	uint32_t reg = FLASH_REG(dev_addr);
//	fprintf(stderr,"fourth cycle reg 0x%08x",reg);
	if(flash->cmd==CMD_AUTOSELECT) {
		for(i=0;i<MAX_AUTO_CODES;i++) {
			AutoSel *auto_sel = &ftype->auto_sel[i];
			if(auto_sel->mask==0) {
				break;
			}
			if((reg & auto_sel->mask)==auto_sel->code) {
				value = auto_sel->value;
				break;
			} 
		}
		return value;
	} else {
		fprintf(stderr,"read fourth unknown cmd %d\n",flash->cmd);
		flash->cycle=CYCLE_FIRST;
		flash->cmd=CMD_NONE;
		return value;
	}
}

/*
 * ------------------------------------
 * Read access in Query mode 
 * ------------------------------------
 */
static uint16_t 
flash_read(void *clientData,uint32_t address,int rqlen) {
	AMD_Flash *flash=clientData;
	AMDFlashBank *bank = flash->bank;
	uint32_t dev_addr=address&(flash->type->size-1);
	//fprintf(stderr,"Flash read %04x cycle %d\n",address,flash->cycle);  
	if(flash->mode==MAP_READ_ARRAY) {
		uint32_t value=0;
		uint8_t *data = (flash->host_mem+FLASH_ADDR_TO_MEMBUS(bank,dev_addr));	
		if((rqlen==4) && ((dev_addr & 3) == 0) ) {
			fprintf(stderr,"Bug: Got 4 Byte request for 16 Bit Chip\n");
		} else if((rqlen==2) && ((dev_addr & 1)==0)) {
			value = *(uint16_t*)data;
		} else if(rqlen==1) {
			value = *(uint8_t*)data;
		} else {
			fprintf(stderr,"illegal access to flash array size %d, addr %08x\n",rqlen,dev_addr);
		}
		return value;
	} else if((flash->mode == MAP_CFI)) {
		unsigned int reg = FLASH_REG((dev_addr ^ bank->endian_xor) & 0xfff);
		return cfi_read(flash->type,reg);
	} else if((flash->mode == MAP_IO)) {
		if(flash->cycle == CYCLE_FIRST) {
			return first_cycle_read(flash,dev_addr ^ bank->endian_xor);
		} else if(flash->cycle == CYCLE_FOURTH) {
			return fourth_cycle_read(flash,dev_addr ^ bank->endian_xor);
		} else {
			fprintf(stderr,"read in cycle %d\n",flash->cycle);
			return 0;
		}
	}
	return 0;
}

/*
 * -------------------------------------------------------
 * Distribute a 8/16 or 32 Bit read access to the chips 
 * of a Flash-Bank 
 * -------------------------------------------------------
 */
static uint32_t 
flashbank_read(void *clientData,uint32_t mem_addr,int rqlen) {
	AMDFlashBank *bank=clientData;
	uint32_t bank_addr = mem_addr & (bank->size-1);
	uint32_t value=0;
	int shift;
	unsigned int flash_addr = MEMBUS_TO_FLASH_ADDR(bank,bank_addr);
	unsigned int chip = (bank_addr>>1) & (bank->nr_chips-1);

	shift=0;
	while(rqlen) {
		if(rqlen>1) {
			value |= flash_read(bank->flash[chip],flash_addr,2)<<shift;
			rqlen-=2;
			chip = (chip+1) & (bank->nr_chips-1);
			if(!chip) {
				flash_addr+=2;
			}
			shift+=16;
		} else {
			value |= flash_read(bank->flash[chip],flash_addr,1)<<shift;
			rqlen--;
		}
	}
	return value;
}

/*
 * -----------------------------------------------------------
 * Memory Mapping functions 
 * -----------------------------------------------------------
 */

static void
FlashBank_Map(void *module_owner,uint32_t base,uint32_t mapsize,uint32_t flags) {
	AMDFlashBank *bank = module_owner;
        uint8_t *host_mem=bank->host_mem;
	int do_mmap = 1;
	int i;
	for(i=0;i<bank->nr_chips;i++) {
		if(bank->flash[i]->mode != MAP_READ_ARRAY) {
			do_mmap = 0;
			break;
		}
	}
	if(do_mmap) {
        	flags &= MEM_FLAG_READABLE;
		Mem_MapRange(base,host_mem,bank->size,mapsize,flags);
	} 
	IOH_NewRegion(base,mapsize,flashbank_read,flashbank_write,HOST_BYTEORDER,bank);
}

static void
FlashBank_UnMap(void *module_owner,uint32_t base,uint32_t mapsize) {
	Mem_UnMapRange(base,mapsize);
	IOH_DeleteRegion(base,mapsize);
}


/*
 * -------------------------------------------
 * Destructor for AMD_Flash
 * -------------------------------------------
 */
void
AMDFlash_Delete(AMD_Flash *flash) {
	free(flash->rmap);
	free(flash);
}

void
AMDFlashBank_Delete(BusDevice *bdev) 
{
	AMDFlashBank *bank=bdev->owner;
	if(bank->disk_image) {
		DiskImage_Close(bank->disk_image);	
		bank->disk_image=NULL;
	} else {
		if(bank->host_mem) {
			free(bank->host_mem);	
		}
	}
	if(bank->stat_image) {
		DiskImage_Close(bank->stat_image);	
		bank->stat_image=NULL;
	}
}

/*
 * ----------------------------------------------------------
 * setup_eraseregions
 * 	Read the Eraseregions from CFI area
 * 	and write it to a table. The order of the  
 * 	table entries is reverted in order to the CFI table for
 * 	Top boot devices
 * ----------------------------------------------------------
 */
static void
setup_eraseregions(AMD_Flash *flash)  
{
	EraseRegion *region;
	FlashType *ftype = flash->type;
	int flashsize=0;
	int i;
	int topboot;
	int n_regions;
	if(!(ftype->capabilities & CAP_CFI)) {
		region = &flash->erase_region[0];
		region->sectorsize = ftype->min_sectorsize;
		region->sectors = ftype->size/region->sectorsize;
		return;
	}
	flash->n_regions = n_regions = cfi_read(ftype,0x2c);
	if(n_regions == 0) {
		region = &flash->erase_region[0];
		flashsize = 1<<cfi_read(ftype,0x27);
		region->sectorsize = flashsize;;
		region->sectors =  1;
	}
	if(n_regions>4) {
		fprintf(stderr,"AMD Flash: Illegal number of Erase Regions: 0x%02x\n",n_regions);
		exit(4325);
	}
	if(ftype->capabilities & CAP_TOPBOOT) {
		topboot=1;
	} else {
		topboot=0;
	}
	for(i=0;i<n_regions;i++) {
		int n_sectors;
		int sectorsize;
		int basereg;
		/* Invert the order for top boot devices ???? */
		if(topboot) {
			basereg = 0x2d+4*(n_regions-i-1);
		} else {
			basereg = 0x2d+4*i;
		}
		n_sectors = (cfi_read(ftype,basereg) | (cfi_read(ftype,basereg+1)<<8))+1;
		sectorsize = 256*(cfi_read(ftype,basereg+2) | (cfi_read(ftype,basereg+3)<<8));
		region = &flash->erase_region[i];
		region->sectorsize = sectorsize;
		region->sectors = n_sectors;
		flashsize += sectorsize*n_sectors;
#if 0
		fprintf(stderr,"Region %d nsectors %d secsize %d size %d\n",i,n_sectors,sectorsize,sectorsize*n_sectors);
#endif
	}
	if((1<<cfi_read(ftype,0x27)) != flashsize) {
		fprintf(stderr,"%s: Mismatch in flashsizes: flashsize %d\n",ftype->name,flashsize);
		exit(25234);
	}
}

/*
 * ------------------------------------------------------------------
 * setup_regionmap
 * 	Setup a mapping table for mapping a address to a erase region
 *	The smallest sector of the chip determines the bits which are
 *	used as index in the mapping table
 * ------------------------------------------------------------------
 */
static void 
setup_regionmap(AMD_Flash *flash) 
{
	int i,j,k,n;	
	int mapsize;
	uint32_t minsectorsize=~0;
	uint32_t flashsize=0;
	int shift;
	EraseRegion *region;
	if(flash->n_regions==0) {
		flash->rmap_size=1;
		return;
	}
	for(i=0;i<flash->n_regions;i++) {
		region = &flash->erase_region[i];
		if(region->sectorsize<minsectorsize) {
			minsectorsize=region->sectorsize;	
		}
		flashsize+=region->sectorsize*region->sectors;
	}
	flash->min_sectorsize = minsectorsize;
	flash->rmap_mask = ~(minsectorsize-1) & (flashsize-1);
	flash->rmap_size = mapsize =  flashsize / minsectorsize;
	for(shift=0;shift<32;shift++) {
		if(((flash->rmap_mask>>shift))&1) {
			break;
		}
	}
	flash->rmap_shift=shift;
	flash->rmap = sg_calloc(sizeof(*flash->rmap) * flash->rmap_size);
	for(n=0,i=0;i<flash->n_regions;i++) {
		int entries_per_sector;
		region = &flash->erase_region[i];
		entries_per_sector = region->sectorsize/minsectorsize;
#if 0
		fprintf(stderr,"Entries per sector %d, region sectors %d\n",entries_per_sector,region->sectors);
#endif
		for(j=0;j<region->sectors;j++) {
			for(k=0;k<entries_per_sector;k++) {
				flash->rmap[n++]=region;	
			}
		}
	}
#if 0	
	fprintf(stderr,"Minimum sectorsize %d\n",minsectorsize);
	fprintf(stderr,"Mapsize %d, n %d\n",mapsize,n);
	fprintf(stderr,"Mask 0x%08x,shift %d\n",flash->rmap_mask,shift);
	sleep(1);
#endif
}

/*
 * ------------------------------------------------------- 
 *  Constructor for AMD_Flash
 * ------------------------------------------------------- 
 */
static AMD_Flash *
AMD_Flash_New(FlashType *ftype,AMDFlashBank *bank,int chip_nr) {
	AMD_Flash *flash = sg_new(AMD_Flash);	

	flash->type = ftype;
	flash->bank = bank;
	flash->host_mem = bank->host_mem + chip_nr * 2; 
	if(bank->statistics_mem) {
		flash->statistics_mem = bank->statistics_mem + chip_nr; // for now, will be changed later
	} else {
		flash->statistics_mem = NULL;
	}
	if(ftype->capabilities & CAP_CFI) {	
		flash->cap_bufwrite = (1<<cfi_read(ftype,0x2a)); 
	}
	flash->cycle=CYCLE_FIRST;
	flash->cmd=CMD_NONE;
	setup_eraseregions(flash);
	setup_regionmap(flash);
	CycleTimer_Init(&flash->erase_timeout,erase_timeout,flash);
	return flash;
}

/*
 * -----------------------------------------------------------
 * change_endian
 *	Endian switching should be part of memory controller
 *      Invoked when the Endian Signal Line changes
 * -----------------------------------------------------------
 */
static void 
change_endian(SigNode *node,int value,void *clientData)
{
        AMDFlashBank *bank = clientData;
        if(value == SIG_HIGH) {
        	fprintf(stderr,"AMD-Flash Warning: Runtime endian switching is broken currently\n");
                fprintf(stderr,"AMD-Flash now big endian\n");
                bank->endian = en_BIG_ENDIAN;
		if(bank->bankwidth == 32) {
			bank->endian_xor = 0;
		} else if(bank->bankwidth == 16) {
			bank->endian_xor = 2;
		} else if(bank->bankwidth == 8) {
			bank->endian_xor = 3;
		} else {
			fprintf(stderr,"Flashbank has illegal bank width %d\n",bank->bankwidth);
		}
		fprintf(stderr,"new xor : %d\n",bank->endian_xor);
        } else if(value==SIG_LOW) {
                bank->endian = en_LITTLE_ENDIAN;
		bank->endian_xor = 0;
        } else {
                fprintf(stderr,"NS9750 Serial: Endian is neither Little nor Big\n");
                exit(3424);
        }
}

/*
 * ---------------------------------------
 * Create a new Flash Bank
 * ---------------------------------------
 */
BusDevice *
AMDFlashBank_New(const char *flash_name) {
	AMDFlashBank *bank = sg_new(AMDFlashBank);
	int i;
	int nr_types = sizeof(flash_types)/sizeof(FlashType);
	uint32_t nr_chips;
	char *mapfile=NULL,*statfile=NULL;
	char *directory;
	char *flash_type;
	FlashType *ftype=NULL;

        flash_type=Config_ReadVar(flash_name,"type");
        if(!flash_type) {
                fprintf(stderr,"Flash bank \"%s\" Flash type not configured\n",flash_name);
		return NULL;
	}
	for(i=0;i<nr_types;i++) {
		ftype=&flash_types[i];	
		if(!strcmp(ftype->name,flash_type)) {
			break;	
		}
	}
	if((i==nr_types)|| !ftype->name) {
		fprintf(stderr,"Flash type %s not found. Available types:\n",flash_type);
		for(i=0;i<nr_types;i++) {
			ftype=&flash_types[i];	
			fprintf(stderr," %-20s - size %dk\n", ftype->name,ftype->size/1024);
		}
		exit(1734);
	}
	if(Config_ReadUInt32(&nr_chips,flash_name,"chips")<0) {
		nr_chips=1;
	}
	if(nr_chips>2) {
		fprintf(stderr,"Illegal number of chips %d in flashbank %s\n",nr_chips,flash_name);
		exit(43652);
	}
	bank->nr_chips = nr_chips;
	if((nr_chips>2) || (nr_chips < 1)) {
		fprintf(stderr,"A AMD-Flash Bank must have 1 or 2 chips\n");
		exit(4534);
	}
	if(nr_chips==2) {
		bank->addr_shift=1;
		bank->bankwidth = 32;
	} else if (nr_chips==1) {
		bank->bankwidth = 16;
		bank->addr_shift=0;
	}
	bank->size = ftype->size * nr_chips;
	directory= Config_ReadVar("global","imagedir");
	if(directory) {
		mapfile = alloca(strlen(directory) + strlen(flash_name)+20);
		sprintf(mapfile,"%s/%s.img",directory,flash_name);
	}
	if(mapfile) {
		bank->disk_image = DiskImage_Open(mapfile,bank->size,DI_RDWR | DI_CREAT_FF); 
		if(!bank->disk_image) {
			fprintf(stderr,"Open disk image failed\n");
			exit(42);
		}
		bank->host_mem=DiskImage_Mmap(bank->disk_image);
	} else {
		bank->host_mem = sg_calloc(bank->size);
		memset(bank->host_mem,0xff,bank->size);
	}
	directory= Config_ReadVar("global","imagedir");
	if(directory) {
		statfile = alloca(strlen(directory) + strlen(flash_name)+20);
		sprintf(statfile,"%s/%s.stat",directory,flash_name);
	}
	if(statfile) {
		int n_sectors = ftype->size / ftype->min_sectorsize;
		int stat_size=sizeof(uint32_t)*n_sectors * nr_chips;
		bank->stat_image = DiskImage_Open(statfile,stat_size,DI_RDWR | DI_CREAT_00);
		if(!bank->stat_image) {
			fprintf(stderr,"Can not open flash statistics file %s\n",statfile);
		} else {
			bank->statistics_mem = DiskImage_Mmap(bank->stat_image);
		}
		
	} 
	/* Now add the chips */
	for(i=0;i<nr_chips;i++) {
		bank->flash[i]=AMD_Flash_New(ftype,bank,i);
	}
	fprintf(stderr,"Flash bank \"%s\" type %s Chips %d writebuf %d\n",
		flash_name,ftype->name,nr_chips,1<<cfi_read(ftype,0x2a));

        bank->big_endianNode = SigNode_New("%s.big_endian",flash_name);
        if(!bank->big_endianNode) {
                fprintf(stderr,"Can not create Flashbank. EndianNode\n");
                exit(3429);
        }
        bank->big_endianTrace = SigNode_Trace(bank->big_endianNode,change_endian,bank);

        bank->bdev.first_mapping=NULL;
        bank->bdev.Map=FlashBank_Map;
        bank->bdev.UnMap=FlashBank_UnMap;
        bank->bdev.owner=bank;
        bank->bdev.hw_flags=MEM_FLAG_READABLE;
	return &bank->bdev;
}
