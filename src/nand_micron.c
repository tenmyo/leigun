
#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include "signode.h"
#include "sgstring.h"
#include "configfile.h"
#include "cycletimer.h"
#include "diskimage.h"

#define NFSTATE_CMD1	(0)
#define NFSTATE_ADDR	(1)
#define	NFSTATE_DATA	(2)
#define NFSTATE_CMD2	(3)

typedef struct FlashDescription {
        char *type;
        int addrbits;
        int colbits;
        uint64_t size;
        uint32_t data_pagesize;
        uint32_t spare_pagesize;
        int planes;
        int blocks;
        int pages_per_block;
        uint32_t us_busy_read;
	uint32_t us_busy_erase;
} FlashDescription;

static FlashDescription flash_types[] = {
        {
                .type = "MT29F16G08CBABB",
                .colbits = 12,
                .addrbits = 26,
                .size = UINT64_C(4320) * 256 * 2048,
                .planes = 2,    /** ??????? */
                .data_pagesize = 4096,
                .spare_pagesize = 224,
                .pages_per_block = 256,
                .blocks = 2048,
                .us_busy_read = 1, /** ????? */
		.us_busy_erase = 15,
        }
};

typedef struct NandFlash {
	int state;
	uint8_t cmd1;
	int addr_cycle;
	uint32_t col_addr;
	uint32_t page_addr;
	uint32_t block_addr;

	FlashDescription *fld;
	DiskImage *diskimage;
	SigNode *sigDQ[8];	
	SigNode *signCE;
	SigNode *sigCLE;
	SigNode *sigALE;
	SigNode *signRE;
	SigNode *signWE;
	SigNode *signWP;
	SigNode *sigRnB;
	SigTrace *traceCE;
	SigTrace *traceCLE;
	SigTrace *traceALE;
	SigTrace *tracenRE;
	SigTrace *tracenWE;
	SigTrace *tracenWP;
} NandFlash;

typedef enum {
	NFEV_CMD,
	NFEV_DATA_IN,
	NFEV_DATA_OUT,
	NFEV_ADDR_IN,
} NFEvent;

/**
 *********************************************************************
 * \fn static uint32_t NF_GetDQ(NandFlash *nf) 
 * Read the state of the Data lines.
 *********************************************************************
 */
static uint32_t
NF_GetDQ(NandFlash *nf) 
{
	uint32_t dq = 0;
	int i;
	for(i = 0; i < 8;i++) {
		if(SigNode_Val(nf->sigDQ[i]) == SIG_HIGH) {
			dq |= (1 << i);
		}
	}
	return dq;
}

#if 0
static void
NF_SetDQ(NandFlash *nf,uint32_t dq) 
{
	int i;
	for(i = 0; i < 8;i++) {
		if((dq >> i) & 1) {
			SigNode_Set(nf->sigDQ[i],SIG_HIGH);
		} else {
			SigNode_Set(nf->sigDQ[i],SIG_LOW);
		}
	}
	return;
}

static void
NF_ReleaseDQ(NandFlash *nf) 
{
	int i;
	for(i = 0; i < 8;i++) {
		SigNode_Set(nf->sigDQ[i],SIG_OPEN);
	}
	return;
}
#endif

static void
NF_Cmd(NandFlash *nf,uint8_t cmd) 
{
	if(nf->state == NFSTATE_CMD1) {
		nf->cmd1 = cmd;
		nf->addr_cycle = 1; 
		switch(cmd) {
			case 0xff:	/* Reset */	
				nf->state = NFSTATE_CMD1;	
				break;	
			case 0xfc: 	/* Synchronous reset */
				nf->state = NFSTATE_CMD1;	
				fprintf(stderr,"Nand flash warning: syncmode not implemented\n");
				break;	
			case 0x90: 	/* Read ID */
				nf->state = NFSTATE_DATA;  
				break;
			case 0xEC: 	/* Read parameter page	*/
				nf->state = NFSTATE_DATA;  
				break;
			
			case 0xED:	/* Read unique id 	*/
				nf->state = NFSTATE_DATA;  
				break;
			case 0xEE:	/* Get features		*/
				nf->state = NFSTATE_DATA;  
				break;
			case 0xEF:	/* Set features 	*/
				nf->state = NFSTATE_DATA;  
				break;
			case 0x70:	/* Read status 		*/	
				nf->state = NFSTATE_DATA;  
				break;
			case 0x78:	/* Read status enhanced	*/
				nf->state = NFSTATE_ADDR;  
				break;
			case 0x05:	/* Change read column	*/
				nf->state = NFSTATE_ADDR;
				break;
			case 0x06:	/* Change read column enhanced */
				nf->state = NFSTATE_ADDR;
				break;	
			case 0x85:	/* Change write colum, Change row address */
					/* copyback program multiplane */
				nf->state = NFSTATE_ADDR;
				break;	
			case 0x00:	/* Read mode, read page,read multiplane,
					  read page cache random copyback read */
				nf->state = NFSTATE_ADDR;
				break;
			case 0x31:	/* Read page cache sequential 	*/
				nf->state = NFSTATE_DATA;	
				break;
			case 0x3F:	/* Read page cache last	*/
				nf->state = NFSTATE_DATA;
				break;
			case 0x80:	/* Program page, page multi plane */
				nf->state = NFSTATE_ADDR;
				break;
			case 0x60:	/* erase block, erase block multi-plane */
				nf->state = NFSTATE_ADDR;
				break;
		}
	} else if((nf->state == NFSTATE_DATA) || (nf->state == NFSTATE_ADDR)) {
		
	}
}

static void
NF_Addr(NandFlash *nf,uint32_t addrbyte) 
{
	switch(nf->addr_cycle) {
		case 1:
			nf->col_addr = addrbyte;
			break;
		case 2:
			nf->col_addr = (nf->col_addr & 0xff) | (addrbyte << 8);
			break;
		case 3:
			nf->page_addr = addrbyte;
			break;
		case 4:
			nf->block_addr = addrbyte << 8;
			break;
		case 5:
			nf->block_addr = (nf->block_addr & 0xffff) | addrbyte << 16;
			break;
		default:
			break;
	}
	nf->addr_cycle++;
}

static void
NF_Data(NandFlash *nf,uint32_t data) 
{
	
}

/**
 ****************************************************************
 * \fn static void CLE_Event(void *eventData)
 * Event handler invoked when CLE (Command latch) changes.
 ****************************************************************
 */
static void
nWE_Event(SigNode *node,int value,void *clientData)

{
	NandFlash *nf = clientData;	
	uint32_t dq;
	int ALE,nCE,CLE,nRE,nWE;
	ALE = SigNode_Val(nf->sigALE);
	nCE = SigNode_Val(nf->signCE);
	CLE = SigNode_Val(nf->sigCLE);
	nRE = SigNode_Val(nf->signRE);
	nWE = SigNode_Val(nf->signWE);
	if(value == SIG_LOW) {
		if((nCE == SIG_LOW) && (ALE == SIG_LOW) && 
		  (CLE == SIG_LOW) && (nWE == SIG_HIGH)) {
			// async data output	
			// Trace WE to disable output
		}
		return;	
	}
	if((nCE == SIG_LOW) && (ALE == SIG_LOW) && 
	  (CLE == SIG_HIGH) && (nRE == SIG_HIGH)) {
		// Async command detected		
		dq = NF_GetDQ(nf);
		NF_Cmd(nf,dq);
	} else if((nCE == SIG_LOW) && (ALE == SIG_HIGH) &&
	  (CLE == SIG_LOW) && (nRE == SIG_HIGH)) {
		// Async Address detected
		dq = NF_GetDQ(nf);
		NF_Addr(nf,dq);
	} else if((nCE == SIG_LOW) && (ALE == SIG_LOW) && 
	  (CLE == SIG_LOW) && (nRE == SIG_HIGH)) {
		// Async data input;
		dq = NF_GetDQ(nf);
		NF_Data(nf,dq);
	}
	/*
 	 */
}

void 
NandFlash_New(const char *name) 
{
	int i;
	int nr_types;
	char *imagedir;
	char *filename;
	char *type;
	NandFlash *nf;
	FlashDescription *fld;
	nr_types = array_size(flash_types);
	imagedir = Config_ReadVar("global","imagedir");
        if(!imagedir) {
                fprintf(stderr,"No directory for NAND Flash diskimage given\n");
                return;
        } 
	type = Config_ReadVar(name,"type");
        if(!type) {
		fprintf(stderr,"No type for Nand Flash \"%s\". Creating nothing\n",name);
		return;
        }
	nf = sg_new(NandFlash);	
        filename = alloca(strlen(imagedir) + strlen(name) + 50);
        sprintf(filename,"%s/%s.img",imagedir,name);
	for(i = 0; i < 8; i++) {
		nf->sigDQ[i] = SigNode_New("%s.DQ%d",name,i);	
	}
	
	nf->signCE = SigNode_New("%s.nCE",name);
	nf->sigCLE = SigNode_New("%s.CLE",name);
	nf->sigALE = SigNode_New("%s.ALE",name);
	nf->signRE = SigNode_New("%s.nRE",name);
	nf->signWE = SigNode_New("%s.nWE",name);
	nf->signWP = SigNode_New("%s.nWP",name);
	nf->sigRnB = SigNode_New("%s.RnB",name);

	if(!nf->signCE || !nf->sigCLE || !nf->signRE || !nf->sigALE ||
	   !nf->signWE || !nf->signWP || !nf->sigRnB) {
		fprintf(stderr,"Can not create signal lines for NAND flash %s\n",name);
		exit(1);
	}
	for(i=0;i < nr_types;i++) {
		fld = &flash_types[i];
		if(!strcmp(type,fld->type)) {
			break;
		}
        }
        if(i == nr_types) {
                fprintf(stderr,"Flash type \"%s\" not found\n",type);
                exit(1);
        }
	nf->fld = fld;

	nf->diskimage = DiskImage_Open(filename,fld->size,DI_RDWR | DI_CREAT_FF | DI_SPARSE);
	if(!nf->diskimage) {
		fprintf(stderr,"Can not open diskimage\n");
		exit(1);
	}
	nf->tracenWE = SigNode_Trace(nf->signWE,nWE_Event,nf);

}
