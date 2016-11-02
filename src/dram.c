/*
 *************************************************************************************************
 * DRAM Emulation
 *
 *  State:
 *	Working but no timing checks. It works even when the 
 *	real CPU will crash because of wrong Number of Row/Column addresses 
 *	or wrong timings.
 *
 *  Used documentation: JEDEC SDRAM3_11_05 SDRAM Architectural Operational Features
 *			Release No.14 3_11_05R14.PDF
 *			Qimonda HYE18L256BFL-7.5 manual 
 *
 * Copyright 2004 2006 Jochen Karrer. All rights reserved.
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
#include <ctype.h>

#include "bus.h"
#include "configfile.h"
#include "dram.h"
#include "sgstring.h"

/* all times in nanoseconds, all clocks in cycles */
typedef struct DRamTiming {
	int tRP; 	// Precharge Command Period		
	int tRAS;	// Active to Precharge 
	int tSREX;	// Self refresh exit time 
	int tAPR;	// Last data out to active time
 	int tDAL;	// Data in to active time
	int tWR;	// Write recovery time
	int tRC;	// Active to active cmd period
	int tRFC;	// Auto Refresh period and refresh to active
	int tXSR;	// exit self refresh to active cmd
	int tRRD;	// active a to active b
	int tMRD;	// Load mode to active command
 	int cCAS;	// CAS latency clocks
} DRamTiming;

typedef struct DRamType {
	char *name;
	uint32_t size_mbits;
	int rowbits;
	int colbits;
	int banks;
	int width;
	DRamTiming timing;
} DRamType;

static const DRamType dram_types[] = {
	{ 
		.name = "K4S2816F32-TC75",	
		.size_mbits = 128,
		.rowbits = 12,
		.colbits = 9, 
		.banks = 4,
		.width = 16,
		.timing = {
			.tRP = 	20,
			.tRAS =	100, 
			.tSREX = 10,
			.tAPR =	10, /* ??? */
			.tDAL =  40,
			.tWR =	20, /* tRDL */
			.tRC =	65,
			.tRFC =	70, /* Infineon */
			.tXSR =	10, /* ????? */
			.tRRD =	15,
			.tMRD =	10,
			.cCAS =   2
		}
	}, 
	{
		.name = "HYE16L256160BF-7.5",
		.size_mbits = 256,
		.rowbits = 13,
		.colbits = 9,
		.banks = 4,
		.width = 16,
		.timing = {
			.tRP = 19,
			.tRAS = 100,
			.tSREX = 10, /* should be 1 clk cycle , not 10 ns */
			.tAPR = 10,  /* guessed, not found in documentation */
			.tDAL = 40,  /* guessed */
			.tWR = 14,
			.tRC = 67,
			.tRFC = 70, /* guessed from other infineon chips */
			.tXSR = 10, /* ??? */
			.tRRD = 15,
			.tMRD = 15, /* should be 2 clock cycles */
			.cCAS = 2,
		}

	}
	
};

/* SDRAM states taken from qimonda HYE18L256160BFL-7.5 docu */
#define SDRSTAT_POWERON		(0)
#define SDRSTAT_DEEPPOWERDOWN	(1)
#define	SDRSTAT_PREALL		(2)
#define SDRSTAT_MODEREGSET	(3)
#define SDRSTAT_SELFREFRESH	(4)
#define SDRSTAT_IDLE		(5)
#define	SDRSTAT_AUTOREFRESH	(6)
#define	SDRSTAT_ACTPWRDWN	(7)
#define	SDRSTAT_PRECHRGPWRDWN	(8)
#define	SDRSTAT_ROWACTIVE	(9)
#define SDRSTAT_CLKSUSPWRT	(10)
#define SDRSTAT_WRITE		(11)
#define SDRSTAT_READ		(12)
#define	SDRSTAT_CLKSUSPRD	(13)
#define SDRSTAT_CLKSUSPWRTA	(14)
#define	SDRSTAT_WRITEA		(15)
#define SDRSTAT_READA		(16)
#define	SDRSTAT_CLKSUSPRDA	(17)
#define SDRSTAT_PRECHRG		(18)

typedef struct DRam {
	BusDevice bdev;
	const DRamType *type;
	int state;
	uint8_t *host_mem;
	uint32_t  size;
	uint32_t flags;
	int cycletype;
} DRam;

#if 0
/*
 * ------------------------------------------------------------------
 * dram_read/dram_write
 *	Write/read SD-RAM
 *	Not used for normal read write cycles because DRAM is in 
 *	mmaped mode in this case. 
 * Shit: this belongs into the dram controller and not into
 * the dram chip 
 * ------------------------------------------------------------------
 */
static uint32_t
dram_read(void *clientData,uint32_t mem_addr,int rqlen) 
{
	DRam *dram = (DRam *)clientData;
	const DRamType *drtype = dram->type;	
	uint32_t raddr,caddr,bank; 
	int rows,int cols,int banks;
	/* if no type specified in configfile return always Ok */
	if(!drtype) {
		return 0;
	}
	rows = (1<<drtype->rowbits);
	cols = (1<<drtype->colbits);
	banks = drtype->banks;
	
	caddr = (addr>>2)  & (cols-1);
	raddr = ((addr>>2) >> drtype->colbits) & (rows-1);
	bank = (addr >> (drtype->rowbits+drtype->colbits)) & (banks-1)
	switch(dram->cycletype) {
		case SDRCYC_NORMAL: 
			fprintf(stderr,"DRAM emulator bug: Normal Cycle in iomapped mode\n");
			break;
		case SDRCYC_PRECHRG:   
			if(raddr & (1<<10))
			
			
		case SDRCYC_AUTOREFRESH: 
		case SDRCYC_SETMODE: 
		case SDRCYC_MANSELFREFRESH:
			break;
		default:
			break;
	}
	return 0;
}

static void
dram_write(void *clientData,uint32_t value, uint32_t mem_addr,int rqlen) 
{
	DRam *dram = (DRam *)clientData;
	fprintf(stderr,"strange write to dram when in a special_cycle type %d\n",dram->cycletype);
}
#endif

static void
DRam_Map(void *module_owner,uint32_t base,uint32_t mapsize,uint32_t flags) {
	DRam *dram = module_owner;
	flags &= MEM_FLAG_READABLE|MEM_FLAG_WRITABLE;
	Mem_MapRange(base,dram->host_mem,dram->size,mapsize,flags);
}

static void
DRam_UnMap(void *module_owner,uint32_t base,uint32_t mapsize) {
	Mem_UnMapRange(base,mapsize); 
}

#if  0
static void 
dram_cmd(DRam *dram,int cmd) {
	int illegal = 0;
	switch(dram->state) {
		case SDRSTAT_POWERON:
			if(cmd == SDRCMD_PREALL) {
				dram->state = SDRSTAT_PREALL;
				dram->state = SDRSTAT_MODEREGSET;
				dram->state = SDRSTAT_IDLE;
			} else {
				illegal = 1;
			}
			break;

		case SDRSTAT_DEEPPOWERDOWN:
			if(cmd == SDRCMD_DPDSX) {
				dram->state = SDRSTAT_POWERON;
			} else {
				illegal = 1;
			} 
			break;

		case SDRSTAT_PREALL:	
			/* ???? */
			if(cmd == SDRCMD_MRS) {
				dram->state = SDRSTAT_IDLE;
			} else {
				illegal=1;
			}
			break;

		case SDRSTAT_MODEREGSET:
			dram->state = SDRSTAT_IDLE;
			fprintf(stderr,"SDRSTAT_MODEREGSET should never be reached\n");
			break;

		case SDRSTAT_SELFREFRESH:
			if(cmd == SDRCMD_REFSX) {
				dram->state = SDRSTAT_IDLE;
			} else {
				illegal = 1;
			}
			break;
		case SDRSTAT_IDLE:
			if(cmd == SDRCMD_ACT) {
				dram->state = SDRSTAT_ROWACTIVE;
			} else if (cmd == SDRCMD_CKEL) {
				dram->state = SDRSTAT_PRECHRGPWRDWN;
			} else if(cmd == SDRCMD_REFA) {
				dram->state = SDRSTAT_AUTOREFRESH;
				dram->state = SDRSTAT_PRECHRG;
				dram->state = SDRSTAT_IDLE;
			} else if(cmd == SDRCMD_REFS) {
				dram->state = SDRSTAT_SELFREFRESH;
			} else if(cmd == SDRCMD_DPDS) {
				dram->state = SDRSTAT_DEEPPOWERDOWN;
			} else if(cmd == SDRCMD_MRS) {
				dram->state = SDRSTAT_MODEREGSET;
			} else if(cmd == SDRCMD_PRE) {
				dram->state = SDRSTAT_PRECHRG;
				dram->state = SDRSTAT_IDLE;
			} else {
				illegal = 1;
			}
			break;
		case SDRSTAT_AUTOREFRESH:
			/* automatically left */
			break;
		case SDRSTAT_ACTPWRDWN:
			if(cmd == SDRCMD_CKEH) {
				dram->state = SDRSTAT_ROWACTIVE;
			} else {
				illegal = 1;
			}
			break;
		case SDRSTAT_PRECHRGPWRDWN:
			if(cmd == SDRCMD_CKEH) {
				dram->state = SDRSTAT_IDLE;
			} else {
				illegal = 1;
			}
			break;
		case SDRSTAT_ROWACTIVE:
			if(cmd == SDRCMD_PRE) {
				dram->state = SDRSTAT_PRECHRG;
				dram->state = SDRSTAT_IDLE;
			}
			break;
		case SDRSTAT_CLKSUSPWRT:
			break;
		case SDRSTAT_WRITE:
			break;
		case SDRSTAT_READ:
			break;
		case SDRSTAT_CLKSUSPRD:
			break;
		case SDRSTAT_CLKSUSPWRTA:
			break;
		case SDRSTAT_WRITEA:
			break;
		case SDRSTAT_READA:
			break;
		case SDRSTAT_CLKSUSPRDA:
			break;
		case SDRSTAT_PRECHRG:
			fprintf(stderr,"PRECHRG should be left automatically\n");
			break;
		default:
			fprintf(stderr,"Illegal SDRAM state\n");
	}
}
#endif

static int 
DRam_SpecialCycle(struct BusDevice *bdev,BusSpecialCycle_t *cyc) 
{
	DRam *dram = bdev->owner; /* container of */
	DRam_SpecialCycle_t *cycle = (DRam_SpecialCycle_t*) cyc;
	int cycletype;
	if(cycle->magic != BSCMAGIC_DRAM_CMD) {
		fprintf(stderr,"Wrong magic in DRam_SpecialCycle\n");
		return -2;
	}
	cycletype = cycle->cycletype;
	if(dram->cycletype == cycletype) {
		return 0;
	}
	if((dram->cycletype == SDRCYC_NORMAL) && (cycletype != SDRCYC_NORMAL)) {
		/* switch to iomap */	
		dram->cycletype = cycletype;
		Mem_AreaUpdateMappings(bdev);
	} else if((dram->cycletype != SDRCYC_NORMAL) && (cycletype == SDRCYC_NORMAL)) {
		/* switch to mmap */
		dram->cycletype = cycletype;
		Mem_AreaUpdateMappings(bdev);
	}
	dram->cycletype = cycletype;
	return 0;
}

static uint32_t
parse_memsize (char *str)
{
        uint32_t size;
        char c;
        if(sscanf(str,"%d",&size)!=1) {
                return 0;
        }
        if(sscanf(str,"%d%c",&size,&c)==1) {
                return size;
        }
        switch(tolower((unsigned char)c)) {
                case 'm':
                        return size*1024*1024;
                case 'k':
                        return size*1024;
        }
        return 0;
}

const DRamType *
find_dram_type(char *typestr) 
{
	const DRamType *type;
	int i;
	for(i=0;i<(sizeof(dram_types)/sizeof(DRamType));i++) {
		type = &dram_types[i];
		if(strcmp(typestr,type->name) == 0) {
			return type;
		}	
	}
	return 0;
}
/*
 * --------------------
 * DRAM New
 * --------------------
 */
BusDevice *
DRam_New(char *dram_name) {
	char *sizestr,*typestr;
	uint32_t size=0;
	uint32_t chips = 0;
	DRam *dram;
	const DRamType *drtype = NULL;
	sizestr=Config_ReadVar(dram_name,"size");
	typestr=Config_ReadVar(dram_name,"type");
	if(sizestr && typestr) {
		fprintf(stderr,"%s: You can specify DRAM size or DRAM type, but not both\n",dram_name);
		exit(1);
	}
	if(sizestr) {
		size=parse_memsize(sizestr);
		if(size==0) {
			fprintf(stderr,"DRAM bank \"%s\" not present\n",dram_name);
			return NULL;
		}
	} else if (typestr) {
		Config_ReadUInt32(&chips,dram_name,"chips");
		drtype = find_dram_type(typestr);	
		if(!drtype) {
			fprintf(stderr,"DRAM %s: type \"%s\" not found\n",dram_name,typestr);
			exit(1);
		}
		if(chips==0) {
			fprintf(stderr,"Number of chips in DRAM bank is not given\n");
			exit(1);
		}
		size = drtype->size_mbits * 1024 * 1024 * chips;
	} else {
		fprintf(stderr,"DRAM bank \"%s\" not present\n",dram_name);
		return NULL;
	}
	dram = sg_new(DRam);
	dram->type = drtype;
	dram->state = SDRSTAT_POWERON;
	if(drtype) {
		/* If user specifies a type then the chip must be programmed */
		dram->cycletype = SDRCYC_UNDEFINED;
	} else {
		/* Skip DRAM initialisation */
		dram->cycletype = SDRCYC_NORMAL;
	}
	dram->host_mem = sg_calloc(size);
	memset(dram->host_mem,0xff,size);
	dram->size=size;
	dram->bdev.first_mapping=NULL;
	dram->bdev.Map=DRam_Map;
	dram->bdev.UnMap=DRam_UnMap;
	dram->bdev.specialCycle=DRam_SpecialCycle;
	dram->bdev.owner=dram;
	dram->bdev.hw_flags=MEM_FLAG_WRITABLE|MEM_FLAG_READABLE;
	fprintf(stderr,"DRAM bank \"%s\" with  size %ukB\n",dram_name,size/1024);
	return &dram->bdev;
}
